// Copyright 2015 Open vStorage NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
