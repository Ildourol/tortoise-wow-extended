/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ARC4.h"
#include "Log.h"

#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
#include <openssl/provider.h>
#ifdef WIN32
#include <windows.h>
#endif
#endif

namespace
{
    // RC4 moved into the legacy provider with OpenSSL 3.0. If that module cannot
    // be found, EVP_rc4() quietly returns nullptr, EVP_EncryptInit_ex leaves the
    // context without a cipher, and EVP_CIPHER_CTX_set_key_length then reads
    // through it - an access violation on the very first login attempt, with
    // nothing in the log to explain it. Both return values used to be discarded.
    EVP_CIPHER const* GetRC4Cipher()
    {
#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
        static bool const legacyReported = []()
        {
#ifdef WIN32
            if (!getenv("OPENSSL_MODULES"))
            {
                char exePath[MAX_PATH];
                if (GetModuleFileNameA(NULL, exePath, MAX_PATH))
                {
                    char* lastSlash = strrchr(exePath, '\\');
                    if (lastSlash)
                    {
                        *lastSlash = '\0';
                        _putenv_s("OPENSSL_MODULES", exePath);
                    }
                }
            }
#endif
            if (!OSSL_PROVIDER_load(nullptr, "legacy"))
                sLog.outError("OpenSSL 3 is in use but its legacy provider could not be loaded. "
                              "RC4 lives there, so session encryption cannot be set up and no client "
                              "will get past the login. Point OPENSSL_MODULES at the directory holding "
                              "legacy.dll (legacy.so on unix), or link against OpenSSL 1.1.");
            OSSL_PROVIDER_load(nullptr, "default");
            return true;
        }();
        (void)legacyReported;
#endif

        EVP_CIPHER const* cipher = EVP_rc4();
        if (!cipher)
            sLog.outError("EVP_rc4() returned nothing - session encryption is unavailable.");

        return cipher;
    }

    // Shared by both constructors: without a cipher the context stays empty
    // rather than half-initialised, so later calls fail instead of crashing.
    void SetUpContext(EVP_CIPHER_CTX* ctx, uint8 len)
    {
        EVP_CIPHER const* cipher = GetRC4Cipher();
        if (!cipher)
            return;

        if (!EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr))
        {
            sLog.outError("EVP_EncryptInit_ex failed for RC4 - session encryption is unavailable.");
            return;
        }

        EVP_CIPHER_CTX_set_key_length(ctx, len);
    }
}

ARC4::ARC4(uint8 len) : m_ctx(nullptr)
{
    m_ctx = EVP_CIPHER_CTX_new();
    SetUpContext(m_ctx, len);
}

ARC4::ARC4(uint8 *seed, uint8 len) : m_ctx(nullptr)
{
    m_ctx = EVP_CIPHER_CTX_new();
    SetUpContext(m_ctx, len);
    EVP_EncryptInit_ex(m_ctx, nullptr, nullptr, seed, nullptr);
}

ARC4::~ARC4()
{
    if (m_ctx)
        EVP_CIPHER_CTX_free(m_ctx);
}

void ARC4::Init(uint8 *seed)
{
    if (m_ctx && EVP_CIPHER_CTX_cipher(m_ctx))
        EVP_EncryptInit_ex(m_ctx, nullptr, nullptr, seed, nullptr);
}

void ARC4::UpdateData(int len, uint8 *data)
{
    if (m_ctx && EVP_CIPHER_CTX_cipher(m_ctx))
    {
        int outlen = 0;
        EVP_EncryptUpdate(m_ctx, data, &outlen, data, len);
        EVP_EncryptFinal_ex(m_ctx, data, &outlen);
    }
}