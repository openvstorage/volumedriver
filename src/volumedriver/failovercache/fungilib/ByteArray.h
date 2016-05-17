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

#ifndef _BYTEARRAY_H
#define	_BYTEARRAY_H

#include "defines.h"
#include <memory>
#include <string>

namespace fungi {

    class ByteArray {
    public:
        ByteArray(){}
        virtual ~ByteArray()
        {}

        virtual operator byte *() = 0;
        virtual operator const byte *() const = 0;

        virtual byte operator [](int index) const = 0;

        virtual byte& operator [](int index) = 0;
        virtual void clear() = 0;
        virtual int32_t size() const = 0;

        virtual void copyFrom(const ByteArray &from) = 0;

        // for when we're interfacing  with non-ByteArray code:
        virtual void copyFrom(const byte *from) = 0;

        std::string toString() const;

        bool compare(const ByteArray &) const;
    };
}

#endif	/* _BYTEARRAY_H */

// Local Variables: **
// End: **
