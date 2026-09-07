# gSOAP runtime source

The SOAP bindings and stdsoap2.h use gSOAP 2.8.135. The previous stdsoap2.cpp was an older incompatible runtime; the upstream Linux archive cannot be linked into our Windows release.

stdsoap2.cpp is the unmodified runtime from Debian's upstream-source archive:
https://deb.debian.org/debian/pool/main/g/gsoap/gsoap_2.8.135.orig.tar.gz

Archive SHA-256: c587d6015dd7ce946d775c7d01ec01a8c2de7f044ec834ed4bf1002e9220fba6

The archive header matches the integrated header after line-ending normalization. CMake builds the source on the target platform; no prebuilt Linux archive is required. See LICENSE.txt and the licensing notice at the top of each source file.
