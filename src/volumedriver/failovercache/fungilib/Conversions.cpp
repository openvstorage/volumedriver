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
