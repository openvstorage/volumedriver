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
