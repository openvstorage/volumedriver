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

#ifndef READABLE_H_
#define READABLE_H_

#include "defines.h"



namespace fungi {
class Readable {
public:
	virtual ~Readable() {}
	/** @exception IOException */
	virtual int32_t read(byte *ptr, int32_t count) = 0;
	virtual bool eof() const = 0;
};
}


#endif /* READABLE_H_ */
