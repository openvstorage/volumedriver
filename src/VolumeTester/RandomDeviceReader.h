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

#ifndef RANDOM_DEVICE_READER_
#define RANDOM_DEVICE_READER_

#include <string.h>
#include <youtils/Logging.h>

class RandomDeviceReader
{
public:
    RandomDeviceReader(const std::string& device,
                       const std::string& reference,
                       const uint64_t num_reads);

    unsigned long long
    size() const;

    bool operator()();

private:
    DECLARE_LOGGER("DeviceReader");

    uint64_t
    nextReadPoint();

    const std::string device_name_;
    const std::string reference_name_;
    static const unsigned blocksize_ = 4096;

    unsigned long long size_;
    uint64_t num_reads_;

    int device_fd_;
    int reference_fd_;
};

#endif // RANDOM_DEVICE_READER_

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
