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
