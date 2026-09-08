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

#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
#include <openssl/provider.h>
#ifdef WIN32
#include <windows.h>
#endif

static void EnsureOpenSSLProviders()
{
    static bool initialized = false;
    if (!initialized)
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
        OSSL_PROVIDER_load(NULL, "legacy");
        OSSL_PROVIDER_load(NULL, "default");
        initialized = true;
    }
}
#endif

ARC4::ARC4(uint8 len) : m_ctx(nullptr)
{
#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
    EnsureOpenSSLProviders();
#endif

    m_ctx = EVP_CIPHER_CTX_new();
    const EVP_CIPHER* cipher = EVP_rc4();
    if (cipher && m_ctx)
    {
        EVP_EncryptInit_ex(m_ctx, cipher, nullptr, nullptr, nullptr);
        EVP_CIPHER_CTX_set_key_length(m_ctx, len);
    }
}

ARC4::ARC4(uint8 *seed, uint8 len) : m_ctx(nullptr)
{
#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
    EnsureOpenSSLProviders();
#endif
    m_ctx = EVP_CIPHER_CTX_new();
    const EVP_CIPHER* cipher = EVP_rc4();
    if (cipher && m_ctx)
    {
        EVP_EncryptInit_ex(m_ctx, cipher, nullptr, nullptr, nullptr);
        EVP_CIPHER_CTX_set_key_length(m_ctx, len);
        EVP_EncryptInit_ex(m_ctx, nullptr, nullptr, seed, nullptr);
    }
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