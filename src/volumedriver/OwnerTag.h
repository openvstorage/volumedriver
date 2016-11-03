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

#ifndef VD_OWNER_TAG_H_
#define VD_OWNER_TAG_H_

#include <youtils/IOException.h>
#include <youtils/OurStrongTypedef.h>

// Support for moving volumes between instances: a changed tag indicates changed
// ownership which could necessitate cache invalidations etc.
// OwnerTag(0) is reserved: it is used internally by volumedriver for backward compat
// purposes but callers must not use it.
OUR_STRONG_NON_ARITHMETIC_TYPEDEF(uint64_t, OwnerTag, volumedriver);

namespace volumedriver
{

MAKE_EXCEPTION(OwnerTagMismatchException,
               fungi::IOException);
}


#endif // !VD_VOLUME_GENERATION_H_
