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

#ifndef _CONVERSIONS_H
#define	_CONVERSIONS_H

#include "defines.h"
#include <string>

// java style type conversions

namespace fungi {
    class Conversions {
    public:
        enum {
            intSize = 4,
            longSize = 8,
            floatSize = 4,
            doubleSize = 8
        };

        static void bytesFromLong(byte bytes[longSize], int64_t val);
        static void bytesFromInt(byte bytes[intSize], int32_t val);
        static void bytesFromboolean(byte bytes[1], bool val);
        static void bytesFromString(byte *bytes, const std::string &val);
        static void longFromBytes(int64_t &val, byte bytes[longSize]);
        static void intFromBytes(int32_t &val, byte bytes[intSize]);
        static void booleanFromBytes(bool &val, byte bytes[1]);
        static void stringFromBytes(std::string &val, byte *bytes, int32_t size);
    };
}


#endif	/* _CONVERSIONS_H */

// Local Variables: **
// mode: c++ **
// End: **
