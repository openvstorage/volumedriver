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

#ifndef WRITABLE_H_
#define WRITABLE_H_

#include "defines.h"

namespace fungi {

class Writable {
public:
	virtual ~Writable() {}
	virtual void open() = 0;
	virtual void close() = 0;
	/** @exception IOException */
	virtual int32_t write(const byte *ptr, int32_t count) = 0;
	virtual int64_t tell() const  = 0;
	virtual int64_t getBufferSize() const {
		return -1;
	}
};

}

#endif /* WRITABLE_H_ */
