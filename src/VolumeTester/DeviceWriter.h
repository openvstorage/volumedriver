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

#ifndef DEVICEWRITER_H_
#define DEVICEWRITER_H_
#include <string>
#include <youtils/Logging.h>
#include "PatternStrategy.h"
#include "stdlib.h"
#include <iosfwd>

class DeviceWriter
{
public:
    DeviceWriter(const std::string& device,
                 const std::string& reference,
                 const uint64_t skipblocks,
                 const uint64_t firstblock,
                 double random,
                 const Strategy& strategy = PatternStrategy());

    ~DeviceWriter();

    unsigned long long
    size() const;

    void
    operator()();

    unsigned long long size_;

    inline bool
    RandomWrite()
    {
        if(random_ >= 0 and random_ < 1)
        {
            return drand48() < random_;
        }
        else
        {
            return true;
        }
    }

    inline bool
    DontSkip(uint64_t currentblock)
    {
    	int pos = currentblock - firstblock_;
    	return pos >= 0 and pos % skipblocks_ == 0;
    }


  private:
    DECLARE_LOGGER("DeviceWriter");

    const std::string device_name_;
    const std::string reference_name_;
    const Strategy& strategy_;

    static const unsigned blocksize_ = 4096;

    int device_fd_;
    int reference_fd_;
    const uint64_t skipblocks_;
    const uint64_t firstblock_;
    double random_;

};

#endif //DEVICEWRITER_H_

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
