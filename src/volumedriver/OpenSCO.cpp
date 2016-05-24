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

#include <boost/thread.hpp>

#include "OpenSCO.h"
#include "CachedSCO.h"
#include "SCOCacheMountPoint.h"
#include "SCOCache.h"

namespace volumedriver
{

namespace bid = boost::interprocess::ipcdetail;
namespace yt = youtils;

void
intrusive_ptr_add_ref(OpenSCO* sco)
{
    bid::atomic_inc32(&sco->refcnt_);
}

void
intrusive_ptr_release(OpenSCO* sco)
{
    // ...::atomic_dec32() (and ..::atomic_inc32() too) returns the old value!
    if (bid::atomic_dec32(&sco->refcnt_) == 1)
    {
        delete sco;
    }
}

namespace
{
inline CreateIfNecessary
from_mode(FDMode mode)
{
    switch (mode)
    {
    case FDMode::Read:
        return CreateIfNecessary::F;
    case FDMode::Write:
    case FDMode::ReadWrite:
        return CreateIfNecessary::T;
    }
    UNREACHABLE;
}
}

OpenSCO::OpenSCO(CachedSCOPtr sco,
                 FDMode mode)
    : sco_(sco)
    , fd_(sco_->path(), mode, from_mode(mode))
    , refcnt_(0)
{
    checkMountPointOnline_();

    // switch (mode)
    // {
    // case FDMode::Read:
    //     fd_.open(sco_->path(), mode, CreateIfNecessary::F);
    //     return;
    // case FDMode::Write:
    // case FDMode::ReadWrite:
    //     fd_.open(sco_->path(), mode, CreateIfNecessary::T);
    //     return;
    // }
}

OpenSCO::~OpenSCO()
{
    try
    {
        // fd_.close();
    }
    catch (std::exception& e)
    {
        LOG_ERROR("Failed to close sco " << sco_->path() << ": " << e.what());
        sco_->getMountPoint()->getSCOCache().reportIOError(sco_);
    }
    catch (...)
    {
        LOG_ERROR("Failed to close sco " << sco_->path() << ": unknown exception");
        sco_->getMountPoint()->getSCOCache().reportIOError(sco_);
    }
}

void
OpenSCO::checkMountPointOnline_() const
{
    if (sco_->getMountPoint()->isOffline())
    {
        throw fungi::IOException("mountpoint is offline",
                                 sco_->getMountPoint()->getPath().string().c_str());
    }
}

ssize_t
OpenSCO::pread(void* buf, size_t count, off_t off)
{
    checkMountPointOnline_();

    return fd_.pread(buf, count, off);
}

ssize_t
OpenSCO::pwrite(const void* buf, size_t count, off_t off, uint32_t& throttle_usecs)
{
    checkMountPointOnline_();

    ssize_t res = fd_.pwrite(buf, count, off);
    if (res == static_cast<ssize_t>(count))
    {
        boost::optional<uint32_t> mpt = sco_->getMountPoint()->getThrottleUsecs();
        if(mpt) {
            throttle_usecs = mpt.get(); }
        else if (sco_->getNamespace()->isChoking()) {
            throttle_usecs = sco_->getMountPoint()->getSCOCache().getThrottleUsecs(); }
        else {
            throttle_usecs = 0;
        }
    }
    else
    {
        throttle_usecs = 0;
    }

    return res;
}

void
OpenSCO::sync()
{
    checkMountPointOnline_();
    fd_.sync();
}

CachedSCOPtr
OpenSCO::sco_ptr() const
{
    return sco_;
}

SCO
OpenSCO::sco_name() const
{
    return sco_->getSCO();
}

void
OpenSCO::purge_from_page_cache()
{
    fd_.fadvise(yt::FAdvise::DontNeed);
}

}

// Local Variables: **
// mode: c++ **
// End: **
