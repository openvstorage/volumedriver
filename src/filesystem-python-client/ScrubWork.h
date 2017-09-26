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

#ifndef VFS_PY_SCRUB_WORK_H_
#define VFS_PY_SCRUB_WORK_H_

namespace volumedriverfs
{

namespace python
{

struct ScrubWork
{
    static void
    registerize();
};

}

}

#endif // !VFS_PY_SCRUB_WORK_H_
