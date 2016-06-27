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

#ifndef _WRAPBYTEARRAY_H
#define	_WRAPBYTEARRAY_H


#include "ByteArray.h"

namespace fungi {
    class WrapByteArray: public ByteArray {
	public:
        explicit WrapByteArray(byte *bytes, int32_t size);
        virtual ~WrapByteArray();

        virtual operator byte *();
        virtual operator const byte *() const;


        virtual byte operator [](int index) const;
        virtual byte& operator [](int index);

        virtual void clear();
        virtual int32_t size() const;

        virtual void copyFrom(const ByteArray &from);

        // for when we're interfacing  with non-ByteArray code:
        virtual void copyFrom(const byte *from);

    private:
        byte *data_;
        int32_t size_;

        // avoid implicit conversions
        WrapByteArray();
        WrapByteArray(const WrapByteArray &);
        WrapByteArray & operator=(const WrapByteArray &);
    };
}


#endif	/* _WRAPBYTEARRAY_H */
