// Copyright 2015 Open vStorage NV
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
