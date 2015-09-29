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

#include "WrapByteArray.h"

#include <cassert>
#include <cstring>

namespace fungi {
    WrapByteArray::WrapByteArray(byte *bytes, int32_t size):data_(bytes), size_(size) {

    }

    WrapByteArray::~WrapByteArray() {

    }

    WrapByteArray::operator byte *() {
        return data_;
    }

    WrapByteArray::operator const byte *() const {
        return data_;
    }

    byte WrapByteArray::operator [](int index) const {
        return data_[index];
    }

    byte& WrapByteArray::operator [](int index)
    {
        return data_[index];
    }

    int32_t WrapByteArray::size() const {
        return size_;
    }

    void WrapByteArray::copyFrom(const ByteArray &from) {
        assert(from.size() == size_);
        const byte *from_b = from;
        memcpy(data_, from_b, size_);
    }

    void WrapByteArray::copyFrom(const byte *from) {
        memcpy(data_, from, size_);
    }

    void WrapByteArray::clear() {
        memset(data_, 0, size_);
    }
}

// Local Variables: **
// End: **
