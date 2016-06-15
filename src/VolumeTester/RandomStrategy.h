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

#ifndef RANDOM_STRATEGY_H_
#define RANDOM_STRATEGY_H_
#include "Strategy.h"
#include <youtils/Logging.h>

static const size_t Cblocksize = 4 * 4096;

class RandomStrategy : public Strategy
{
  public:
    RandomStrategy();

    virtual ~RandomStrategy();

    virtual void
    operator()(unsigned char* buf1,
               unsigned long blocksize) const;

  private:
    DECLARE_LOGGER("RandomStrategy");
    int fd;
    char buf[Cblocksize];
};

#endif // RANDOM_STRATEGY_H_

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
