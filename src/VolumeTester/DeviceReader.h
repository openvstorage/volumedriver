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

#ifndef DEVICE_READER_H
#define DEVICE_READER_H

#include <string>
#include <youtils/Logging.h>

class DeviceReader
{
  public:
    DeviceReader(const std::string& device,
                 const std::string& reference,
                 const double read_chance = 1.0);

    unsigned long long
    size() const;

    bool operator()();

  private:
    DECLARE_LOGGER("DeviceReader");

    bool
    shouldRead();

    const std::string device_name_;
    const std::string reference_name_;
    static const unsigned blocksize_ = 4096;

    unsigned long long size_;
    double read_chance_;

    int device_fd_;
    int reference_fd_;
};

#endif // DEVICE_READER_H

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
