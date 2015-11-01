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

#include "Conversions.h"

#include <cstring>

namespace fungi {

    void Conversions::bytesFromLong(byte bytes[longSize], int64_t val) {
        int64_t *p = (int64_t*)&bytes[0];
        *p = val;
    }

    void Conversions::bytesFromInt(byte bytes[intSize], int32_t val) {
        int32_t *p = (int32_t*)&bytes[0];
        *p = val;
    }

    void Conversions::bytesFromboolean(byte bytes[1], bool val) {
        bytes[0] = val;
    }

    void Conversions::bytesFromString(byte *bytes, const std::string &val) {
        std::string::size_type s = val.length();
        const char *valc = val.c_str();
        memcpy(bytes, valc, s);
        bytes[s] = '\0';
    }

    void Conversions::longFromBytes(int64_t &val, byte bytes[longSize]) {
        int64_t *p = (int64_t *)&bytes[0];
        val = *p;
    }

    void Conversions::intFromBytes(int32_t &val, byte bytes[intSize]) {
        int32_t *p = (int32_t *)&bytes[0];
        val = *p;
    }

    void Conversions::booleanFromBytes(bool &val, byte bytes[1]) {
        val = bytes[0]?true:false;
    }

    void Conversions::stringFromBytes(std::string &val,
				      byte *bytes, int32_t size) {
        val.clear();
        val.append(reinterpret_cast<const char *> (bytes), size);
    }
}
