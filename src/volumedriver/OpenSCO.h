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

#ifndef OPEN_SCO_H_
#define OPEN_SCO_H_
#include <boost/interprocess/detail/atomic.hpp>

#include <youtils/FileDescriptor.h>
#include "SCO.h"

namespace volumedriver
{

using ::youtils::FDMode;

// TODO: use a fast allocator (boost? loki?) for this.
class OpenSCO

{
    friend class CachedSCO;

public:
    ~OpenSCO();

    OpenSCO(const OpenSCO&) = delete;
    OpenSCO& operator=(const OpenSCO&) = delete;

    ssize_t
    pread(void* buf, size_t count, off_t offset);

    ssize_t
    pwrite(const void* buf, size_t count, off_t offset, uint32_t& throttle_usecs);

    void
    sync();

    // Z42: find a better name
    SCO
    sco_name() const;

    CachedSCOPtr
    sco_ptr() const;

private:
    DECLARE_LOGGER("OpenSCO");
    CachedSCOPtr sco_;
    ::youtils::FileDescriptor fd_;

    volatile boost::uint32_t refcnt_;

    OpenSCO(CachedSCOPtr sco,
            FDMode mode);

    void
    checkMountPointOnline_() const;

    friend void
    intrusive_ptr_add_ref(OpenSCO *);

    friend void
    intrusive_ptr_release(OpenSCO *);
};

}

#endif // !OPEN_SCO_H_
// Local Variables: **
// mode: c++ **
// End: **
