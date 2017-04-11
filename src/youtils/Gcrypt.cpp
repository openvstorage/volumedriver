// Copyright (C) 2016 iNuron NV
//
// This file is part of Open vStorage Open Source Edition (OSE),
// as available from
//
//      http://www.openvstorage.org and
//      http://www.openvstorage.com.
//
// This file is free software; you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License v3 (GNU AGPLv3)
// as published by the Free Software Foundation, in version 3 as it comes in
// the LICENSE.txt file of the Open vStorage OSE distribution.
// Open vStorage is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY of any kind.

#include "Gcrypt.h"
#include "IOException.h";

#include <iostream>
#include <memory>

#include <gcrypt.h>

GCRY_THREAD_OPTION_PTHREAD_IMPL;

namespace
{
uint8_t AES256_KEY_SIZE = 32;
uint8_t AES256_BLOCK_SIZE = 16;
}

namespace youtils
{

void
Gcrypt::init_gcrypt()
{
    gcry_error_t err;
    if (gcry_control(GCRYCTL_ANY_INITIALIZATION_P))
    {
        return;
    }

    err = gcry_control(GCRYCTL_SET_THREAD_CBS,
                       &gcry_threads_pthread);
    if (err)
    {
        std::cerr << "gcry_control(GCRYCTL_SET_THREAD_CBS) failed: "
                  << gcry_strerror(err);
        throw fungi::IOException("gcry_control failed)");
    }
    gcry_check_version(GCRYPT_VERSION);
    gcry_control (GCRYCTL_DISABLE_SECMEM, 0);
    gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);
}

std::string
Gcrypt::encrypt(const std::string& buf)
{

    gcry_error_t gcryError;
    gcry_cipher_hd_t gcryCipherHd;

    gcryError = gcry_cipher_open(&gcryCipherHd,
                                 GCRY_CIPHER_AES256,
                                 GCRY_CIPHER_MODE_ECB,
                                 GCRY_CIPHER_CBC_CTS);
    if (gcryError)
    {
        std::cerr << "gcry_cipher_open failed: "
                  << gcry_strsource(gcryError) << "/"
                  << gcry_strerror(gcryError);
        throw GcryOpenException("gcry_cipher_open");
    }

    gcryError = gcry_cipher_setkey(gcryCipherHd,
                                   aes_key.c_str(),
                                   AES256_KEY_SIZE);
    if (gcryError)
    {
        std::cerr << "gcry_cipher_setkey failed: "
                  << gcry_strsource(gcryError) << "/"
                  << gcry_strerror(gcryError);
        throw GcryCipherSetKeyException("gcry_cipher_setkey");
    }

    gcryError = gcry_cipher_setiv(gcryCipherHd,
                                  init_vector.c_str(),
                                  AES256_BLOCK_SIZE);
    if (gcryError)
    {
        std::cerr << "gcry_cipher_setiv failed: "
                  << gcry_strsource(gcryError) << "/"
                  << gcry_strerror(gcryError);
        throw GcryCipherSetIVException("gcry_cipher_setiv");
    }

    size_t blocks = buf.length() / AES256_BLOCK_SIZE;
    if (buf.length() % AES256_BLOCK_SIZE != 0)
    {
        blocks++;
    }

    size_t total_block_size = blocks * AES256_BLOCK_SIZE;

    std::unique_ptr<char[]> encoded(new char[total_block_size]());
    memset(encoded.get(), 0x0, total_block_size);
    memcpy(encoded.get(), buf.data(), buf.length());

    gcryError = gcry_cipher_encrypt(gcryCipherHd,
                                    encoded.get(),
                                    total_block_size,
                                    NULL,
                                    0);
    if (gcryError)
    {
        std::cerr << "gcry_cipher_encrypt failed: "
                  << gcry_strsource(gcryError) << "/"
                  << gcry_strerror(gcryError);
        throw GcryCipherEncryptException("gcry_cipher_encypt");
    }
    gcry_cipher_close(gcryCipherHd);
    std::string enc(encoded.get(), total_block_size);
    return enc;
}

std::string
Gcrypt::decrypt(const std::string& buf)
{
    gcry_error_t gcryError;
    gcry_cipher_hd_t gcryCipherHd;

    gcryError = gcry_cipher_open(&gcryCipherHd,
                                 GCRY_CIPHER_AES256,
                                 GCRY_CIPHER_MODE_ECB,
                                 GCRY_CIPHER_CBC_CTS);
    if (gcryError)
    {
        std::cerr << "gcry_cipher_open failed: "
                  << gcry_strsource(gcryError) << "/"
                  << gcry_strerror(gcryError);
        throw GcryOpenException("gcry_cipher_open");
    }

    gcryError = gcry_cipher_setkey(gcryCipherHd,
                                   aes_key.c_str(),
                                   AES256_KEY_SIZE);
    if (gcryError)
    {
        std::cerr << "gcry_cipher_setkey failed: "
                  << gcry_strsource(gcryError) << "/"
                  << gcry_strerror(gcryError);
        throw GcryCipherSetKeyException("gcry_cipher_setkey");
    }

    gcryError = gcry_cipher_setiv(gcryCipherHd,
                                  init_vector.c_str(),
                                  AES256_BLOCK_SIZE);
    if (gcryError)
    {
        std::cerr << "gcry_cipher_setiv failed: "
                  << gcry_strsource(gcryError) << "/"
                  << gcry_strerror(gcryError);
        throw GcryCipherSetIVException("gcry_cipher_setiv");
    }

    std::unique_ptr<char[]> decoded(new char[buf.length() + 1]());
    memset(decoded.get(), 0x0, buf.length() + 1);
    memcpy(decoded.get(), buf.data(), buf.length());

    gcryError = gcry_cipher_decrypt(gcryCipherHd,
                                    decoded.get(),
                                    buf.length(),
                                    NULL,
                                    0);
    if (gcryError)
    {
        std::cerr << "gcry_cipher_decrypt failed: "
                  << gcry_strsource(gcryError) << "/"
                  << gcry_strerror(gcryError);
        throw GcryCipherEncryptException("gcry_cipher_encypt");
    }
    gcry_cipher_close(gcryCipherHd);
    std::string dec(decoded.get());
    return dec;
}

}
