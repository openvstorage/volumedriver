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

#include "Weed.h"
#include "Assert.h"

namespace youtils
{
static_assert(sizeof(Weed) == Weed::weed_size, "Strangeness");

namespace
{
inline uint8_t
getHexNum(char i)
{
    switch(i)
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return (i - '0');

    case 'a':
    case 'A':
        return 0xa;

    case 'b':
    case 'B':
        return 0xb;

    case 'c':
    case 'C':
        return 0xc;

    case 'd':
    case 'D':
        return 0xd;

    case 'e':
    case 'E':
        return 0xe;

    case 'f':
    case 'F':
        return 0xf;

    default:
        //        LOG_ERROR("Not a valid hexadecimal character " << val);
        throw fungi::IOException("Not a valid hexadecimal character ");
    }
}
}

Weed::Weed(const byte* input, const uint64_t input_size)
{
    // Allocating this locally makes it 0.5 seconds per million weeds
    // then allocating once globally
    //CryptoPP::Weak::MD5 md5;
    // md5.CalculateDigest(weed_,
    //                     input,
    //                     input_size);
    MD5(input,
        input_size,
        weed_);
}

Weed::Weed(std::istream& is)
{
    MD5_CTX ctx;
    int ret = MD5_Init(&ctx);
    if (ret == 0)
    {
        throw fungi::IOException("MD5_Init failed");
    }

    std::vector<char> buf(stream_chunk_size_);

    while (not is.eof())
    {
        is.read(buf.data(),
                buf.size());
        if (is.fail() and not is.eof())
        {
            throw fungi::IOException("Failed to read bytes from stream for MD5");
        }

        ret = MD5_Update(&ctx,
                         buf.data(),
                         is.gcount());
        if (ret == 0)
        {
            throw fungi::IOException("MD5_Update failed");
        }
    }

    ret = MD5_Final(weed_,
                    &ctx);

    if (ret == 0)
    {
        throw fungi::IOException("MD5_Final failed");
    }
}

Weed::Weed(const std::vector<uint8_t>& input)
{
    //    CryptoPP::Weak::MD5 md5;
    // md5.CalculateDigest(weed_,
    //                     input.empty() ? nullptr : &input[0],
    //                     input.size());
    MD5(input.data(),
        input.size(),
        weed_);
}

Weed::Weed(const std::string& str)
{
    VERIFY(str.length() == 2 * weed_size);

    for(unsigned i = 0; i < weed_size; ++i)
    {
        weed_[i] = (getHexNum(str[2 * i]) << 4) + getHexNum(str[2 * i + 1]);
    }
}

bool
Weed::check(const byte* input,
            const uint64_t input_size) const
{
    // CryptoPP::Weak::MD5 md5;
    // return md5.VerifyDigest(weed_,
    //                         input,
    //                         input_size);
    uint8_t weed[weed_size];
    MD5(input,
        input_size,
        weed);
    return memcmp(weed_ , weed, weed_size) == 0;
}

bool
Weed::operator==(const Weed& inother) const
{
    return memcmp(weed_ ,inother.weed_, weed_size) == 0;
}

}

// std::ostream&
// operator<<(std::ostream& ostr,
//            const youtils::Weed& weed)
// {
//     return weed.print(ostr);
// }

std::basic_ostream<char>&
operator<<(std::basic_ostream<char>& ostr,
           const youtils::Weed& weed)
{
    return weed.print(ostr);
}

// Local Variables: **
// mode: c++ **
// End: **
