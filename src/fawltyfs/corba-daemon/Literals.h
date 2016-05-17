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

#ifndef _LITERALS_H_
#define _LITERALS_H_

#define TEST_NAME "test"
#define TEST_KIND "context"

#define OBJECT_NAME "fawlty"
#define OBJECT_KIND "Object"
#include "fawlty.hh"
#include "../FileSystem.h"
fawltyfs::FileSystemCall
fromCorba(const Fawlty::FileSystemCall in);

Fawlty::FileSystemCall
toCorba(fawltyfs::FileSystemCall in);

std::set<fawltyfs::FileSystemCall>
fromCorba(const Fawlty::FileSystemCalls& calls);

Fawlty::FileSystemCalls
toCorba(const std::set<fawltyfs::FileSystemCall>& calls);

#endif // _LITERALS_H_
