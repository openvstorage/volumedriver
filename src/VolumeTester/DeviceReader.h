// Copyright 2015 iNuron NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
