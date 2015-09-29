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
