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
