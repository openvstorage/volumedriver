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

#ifndef YT_GCRYPT_ENCRYPT_DECRYPT_H_
#define YT_GCRYPT_ENCRYPT_DECRYPT_H_

#include "IOException.h"

#include <gcrypt.h>
#include <string>

MAKE_EXCEPTION(GcryOpenException, fungi::IOException);
MAKE_EXCEPTION(GcryCipherSetKeyException, fungi::IOException);
MAKE_EXCEPTION(GcryCipherSetIVException, fungi::IOException);
MAKE_EXCEPTION(GcryCipherEncryptException, fungi::IOException);

namespace youtils
{

class Gcrypt
{
public:
    Gcrypt(const std::string& aes_key_,
           const std::string& init_vector_)
    : aes_key(aes_key_)
    , init_vector(init_vector_)
    {}

    static int
    init_gcrypt();

    std::string
    encrypt(const std::string& buf);

    std::string
    decrypt(const std::string& buf);
private:
    std::string aes_key;
    std::string init_vector;

};

}

#endif // !YT_GCRYPT_ENCRYPT_DECRYPT_H_
