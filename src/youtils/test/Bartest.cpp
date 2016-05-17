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

#include "../TestBase.h"

#include <boost/io/ios_state.hpp>

namespace youtilstest
{

class Bartest
    : public TestBase
{};

struct CL
{
    uint16_t word;
    uint8_t b1;
    uint32_t lungo;
    uint8_t b2;
} __attribute__ ((__packed__));

TEST_F(Bartest, DISABLED_test1)
{

    uint64_t a = 0xAABBBBBBBBCCDDDDULL;

    CL* cl = reinterpret_cast<CL*>(&a);

    boost::io::ios_all_saver osg(std::cout);

    std::cout << std::hex << "word: " << cl->word << std::endl
    << "b1: " << static_cast<int>(cl->b1) << std::endl
    << "lungo: " << cl->lungo << std::endl
    << "b2: "  << static_cast<int>(cl->b2) << std::endl;

    uint64_t b = a >> 16;

    cl = reinterpret_cast<CL*>(&b);

    std::cout << std::hex << "word: " << cl->word << std::endl
    << "b1: " << static_cast<int>(cl->b1) << std::endl
    << "lungo: " << cl->lungo << std::endl
    << "b2: "  << static_cast<int>(cl->b2) << std::endl;

    std::cout << ((b >> 40) bitand 0xff) << std::endl;
}

}

// Local Variables: **
// mode: c++ **
// End: **
