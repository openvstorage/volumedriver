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

#include "Buffer.h"
#include <youtils/IOException.h>
#include <memory.h>

#define _GRANULARITY_ 8192

namespace fungi {

Buffer::Buffer()
    : capacity_(0)
    , buf_(nullptr)
    , size_(0)
    , read_(0)
{
}

int Buffer::round(int needed)
{
    return ((needed + (_GRANULARITY_ - 1)) / _GRANULARITY_) * _GRANULARITY_;
}

void Buffer::append(byte *buf, int size)
{
    if (size_ + size > capacity_)
    {
        capacity_ = round(capacity_ + size);
        vec_.resize(capacity_);
        buf_ = vec_.data();
    }
    memcpy(buf_ + size_, buf, size);
    size_ += size;
}

void Buffer::clear()
{
    size_ = 0;
}

byte * Buffer::data()
{
    return buf_;
}

int Buffer::size()
{
    return size_;
}

int Buffer::capacity()
{
    return capacity_;
}

byte * Buffer::begin()
{
    return buf_;
}

byte * Buffer::end()
{
    return buf_ + size_;
}

void Buffer::store(Streamable& s, int size) {
//    printf("buffer[%d] store %d\n", size_ - read_, size);
    if (size > capacity_)
    {
        capacity_ = round(capacity_ + size);
        vec_.resize(capacity_);
        buf_ = vec_.data();
    }

    size_ = s.read(buf_, (int32_t) size);
    read_ = 0;
}

int Buffer::read(byte *buf, int n)
{
    int remaining = size_ - read_;
//    printf("buffer[%d] read %d\n", remaining, n);
    if (n > remaining) {
        throw IOException("Buffer::read attempt to read more bytes than buffered");
    }
    memcpy(buf, buf_ + read_, n);
    remaining -= n;
    if (remaining == 0) {
        size_ = 0;
        read_ = 0;
    }
    else {
        read_ += n;
    }
    return n;
}

} // namespace fungi
