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

#include "ByteArray.h"
#include <sstream>
#include <cassert>
#include <limits>
#include <cstring>

namespace fungi {


std::string ByteArray::toString() const {
	std::stringstream ss;
	ss << std::hex;
	int32_t sizex = size();
	assert(sizex < std::numeric_limits<int>::max());
	for (int i=0; i < (int)sizex; ++i) {
		const byte b = (*this)[i];
		const byte bh = b >> 4;
		const byte bl = b & 15;
		ss << (int)bh << (int)bl;
	}
	std::string s = ss.str();
	return s;
}

bool ByteArray::compare(const ByteArray &a2) const
{
    if (size() != a2.size())
    {
        return false;
    }
    return memcmp(a2.operator const byte*(), operator const byte*(), size()) < 0;
}

}

// Local Variables: **
// End: **
