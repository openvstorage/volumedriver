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

    void
    purge_from_page_cache();

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
