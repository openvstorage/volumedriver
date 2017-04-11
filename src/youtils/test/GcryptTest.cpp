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

#include "../Gcrypt.h"

#include <boost/lexical_cast.hpp>
#include <boost/optional/optional_io.hpp>

#include <gtest/gtest.h>

namespace youtilstest
{

namespace
{
std::string aes_key("gcrypt test aes key init vector.");
std::string init_vector("gcrypt test init");
}

using namespace std::literals::string_literals;
using namespace youtils;


class GcryptTest
    : public testing::Test
{
public:
    GcryptTest()
    {}

    void
    SetUp()
    {
        try
        {
            Gcrypt::init_gcrypt();
        }
        catch (const std::exception& e)
        {
            std::cerr << "gcrypt_init failed: " << e.what();
            abort();
        }
        catch (...)
        {
            std::cerr << "gcrypt_init failed with unknown error\n";
            abort();
        }
    }

    void
    TearDown()
    {}
};

TEST_F(GcryptTest, lt_block_size)
{
    const std::string message("bob");

    std::string encrypted(Gcrypt(aes_key, init_vector).encrypt(message));
    std::string decrypted(Gcrypt(aes_key, init_vector).decrypt(encrypted));

    EXPECT_EQ(message, decrypted);
}

TEST_F(GcryptTest, eq_block_size)
{
    const std::string message("cnanakoscnanakos");

    std::string encrypted(Gcrypt(aes_key, init_vector).encrypt(message));
    std::string decrypted(Gcrypt(aes_key, init_vector).decrypt(encrypted));

    EXPECT_EQ(message, decrypted);
}

TEST_F(GcryptTest, gt_block_size)
{
    const std::string message("cnanakoscnanakos12345");

    std::string encrypted(Gcrypt(aes_key, init_vector).encrypt(message));
    std::string decrypted(Gcrypt(aes_key, init_vector).decrypt(encrypted));

    EXPECT_EQ(message, decrypted);
}

TEST_F(GcryptTest, big)
{
    const std::string message("o_k)m2406u66kB}ed57'h\"5$w'LP'9");

    std::string encrypted(Gcrypt(aes_key, init_vector).encrypt(message));
    std::string decrypted(Gcrypt(aes_key, init_vector).decrypt(encrypted));

    EXPECT_EQ(message, decrypted);
}

TEST_F(GcryptTest, very_big)
{
    const std::string
    message("$UHT{/Hy3H417>geJ\"36(G7h]6@5*Z5z!_Ic6Ip2f1HC9deaX6o0bh9Uf)HCnj4\"}");

    std::string encrypted(Gcrypt(aes_key, init_vector).encrypt(message));
    std::string decrypted(Gcrypt(aes_key, init_vector).decrypt(encrypted));

    EXPECT_EQ(message, decrypted);
}

}
