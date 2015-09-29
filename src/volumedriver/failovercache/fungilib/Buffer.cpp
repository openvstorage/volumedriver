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

void Buffer::store(Streamable *s, int size) {
//    printf("buffer[%d] store %d\n", size_ - read_, size);
    if (size > capacity_)
    {
        capacity_ = round(capacity_ + size);
        vec_.resize(capacity_);
        buf_ = vec_.data();
    }

    size_ = s->read(buf_, (int32_t) size);
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
