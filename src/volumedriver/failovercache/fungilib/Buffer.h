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

#ifndef _fungilib_BUFFER_H
#define _fungilib_BUFFER_H

#include "Streamable.h"

namespace fungi {

class Buffer
{
private:
    std::vector<byte> vec_;
    int capacity_;
    byte *buf_;
    int size_;
    int read_;
public:
    Buffer();
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    int round(int needed);
    void append(byte *buf, int size);
    void clear();
    byte *data();
    int size();
    int capacity();
    byte *begin();
    byte *end();
    void store(Streamable& s, int n);
    int read(byte *buf, int n);
};

} // namespace fungi

#endif
