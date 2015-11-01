// Copyright 2015 iNuron NV
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

#ifndef BUG_TWO_H
#define BUG_TWO_H
#include "Bug1.h"

class Bug2
{
public:
    Bug2() = default;

    std::string
    call(const Bug1&)
    {
        //        print "Bug2::call";
    }

    std::string
    str()
    {
        return "Bug2";
    }

};

#endif // BUG_TWO_H
