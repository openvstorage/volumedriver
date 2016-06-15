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

#include "RandomStrategy.h"
#include "VTException.h"
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


RandomStrategy::RandomStrategy()
{
    mode_t mode = 0;
    fd = open("/dev/urandom",O_RDONLY,mode);
    if(fd < 0)
    {
        throw VTException("Could not open file");
    }
    ssize_t red = read(fd,buf,Cblocksize);
    if(red != static_cast<const ssize_t>(Cblocksize))
    {
        throw VTException("Could not read enough data from /dev/urandom");
    }

}


RandomStrategy::~RandomStrategy()
{
    close(fd);
}



void
RandomStrategy::operator()(unsigned char* buf1,
                           unsigned long blocksize) const
{
    assert(blocksize <= Cblocksize);
    memcpy(buf1,buf,blocksize);
}

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
