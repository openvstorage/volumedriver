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

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include <youtils/IOException.h>

#include "CachedSCO.h"
#include "SCOCacheMountPoint.h"
#include "SCOCacheNamespace.h"
#include "SCOCache.h"
#include "OpenSCO.h"
#include <boost/interprocess/detail/atomic.hpp>
namespace volumedriver
{

namespace bid = boost::interprocess::ipcdetail;

void
intrusive_ptr_add_ref(CachedSCO* sco)
{
    bid::atomic_inc32(&sco->refcnt_);
}

void
intrusive_ptr_release(CachedSCO* sco)
{
    // ...::atomic_dec32() (and ..::atomic_inc32() too) returns the old value!
    if (bid::atomic_dec32(&sco->refcnt_) == 1)
    {
        delete sco;
    }
}

namespace {
const std::string
makeFileName(const Namespace& nsName,
             SCO scoName,
             const fs::path& cachePath)
{
    fs::path p(cachePath / nsName.str());
    p /= scoName.str();
    return p.string();
}
}

CachedSCO::CachedSCO(SCOCacheNamespace* nspace,
                     SCO scoName,
                     SCOCacheMountPointPtr mntPoint,
                     const fs::path& path)
    : path_(path)
    , nspace_(nspace)
    , scoName_(scoName)
    , mntPoint_(mntPoint)
    , size_(0)
    , xVal_(0)
    , disposable_(false)
    , unlink_on_destruction_(false)
    , refcnt_(0)
{
    checkMountPointOnline_();

    struct stat st;
    int ret = ::stat(path_.string().c_str(), &st);
    if (ret < 0)
    {
        ret = errno;
        LOG_ERROR("Failed to stat " << path_ << ": " << strerror(ret));
        throw fungi::IOException("Failed to stat",
                                 path_.string().c_str(),
                                 ret);
    }

    size_ = st.st_size;
    disposable_ = (st.st_mode & S_ISVTX) != 0;
}

CachedSCO::CachedSCO(SCOCacheNamespace* nspace,
                     SCO scoName,
                     SCOCacheMountPointPtr mntPoint,
                     uint64_t maxSize,
                     float xVal)
    : path_(makeFileName(nspace->getName(),
                         scoName,
                         mntPoint->getPath()))
    , nspace_(nspace)
    , scoName_(scoName)
    , mntPoint_(mntPoint)
    , size_(maxSize)
    , xVal_(xVal)
    , disposable_(false)
    , unlink_on_destruction_(false)
    , refcnt_(0)
{
    if (size_ == 0)
    {
        LOG_ERROR("attempt to create sco " << path_ << " with capacity 0");

        throw fungi::IOException("attempt to create sco with capacity 0",
                                 path_.string().c_str(),
                                 EINVAL);
    }

    mntPoint_->updateUsedSize(size_);
}

CachedSCO::~CachedSCO()
{
    if (unlink_on_destruction_)
    {
        try
        {
            fs::remove(path_);
            mntPoint_->updateUsedSize(-size_);
        }
        catch (std::exception& e)
        {
            LOG_ERROR("Failed to unlink " << path_ << ": " << e.what());
        }
        catch (...)
        {
            LOG_ERROR("Failed to unlink " << path_ << ": unknown exception");
        }
    }
}

bool
CachedSCO::isDisposable() const
{
    return disposable_;
}

void
CachedSCO::setDisposable()
{
    checkMountPointOnline_();

    struct stat st;
    int ret = ::stat(path_.string().c_str(), &st);
    if (ret < 0)
    {
        ret = errno;
        LOG_ERROR("Failed to stat " << path_ << ": " << strerror(ret));
        throw fungi::IOException("Failed to stat",
                                 path_.string().c_str(),
                                 ret);
    }

    if (!st.st_size)
    {
        LOG_ERROR("attempt to set empty SCO disposable: " << path_);
        throw fungi::IOException("attempt to set empty SCO disposable",
                                 path_.string().c_str());
    }

    ret = ::chmod(path_.string().c_str(), st.st_mode ^ S_ISVTX);
    if (ret < 0)
    {
        ret = errno;
        LOG_ERROR("Failed to chmod " << path_ << ": " << strerror(ret));
        throw fungi::IOException("Failed to chmod",
                                 path_.string().c_str(),
                                 ret);
    }

    int64_t diff = st.st_size - size_;

    size_ = st.st_size;
    disposable_ = true;
    mntPoint_->updateUsedSize(diff);
}

SCO
CachedSCO::getSCO() const
{
    return scoName_;
}

uint64_t
CachedSCO::getSize() const
{
    return size_;
}

uint64_t
CachedSCO::getRealSize() const
{
    //currently only monitoring call, we ignore fs errors and do not offline mountpoints
    if (disposable_)
    {
        return size_;
    }
    else
    {
        struct stat st;
        int ret = ::stat(path_.string().c_str(), &st);

        if (ret < 0 or !st.st_size)
        {
            return size_;
        }
        else
        {
            return st.st_size;
        }
    }
}

float
CachedSCO::getXVal() const
{
    return xVal_;
}

void
CachedSCO::setXVal(float xVal)
{
    xVal_ = xVal;
}

SCOCacheNamespace*
CachedSCO::getNamespace() const
{
    return nspace_;
}

SCOCacheMountPointPtr
CachedSCO::getMountPoint()
{
    return mntPoint_;
}

OpenSCOPtr
CachedSCO::open(FDMode mode)
{
    return OpenSCOPtr(new OpenSCO(this, mode));
}

void
CachedSCO::incRefCount(uint32_t num)
{
    mntPoint_->getSCOCache().signalSCOAccessed(this, num);
}

uint32_t
CachedSCO::use_count()
{
    return bid::atomic_read32(&refcnt_);
}

void
CachedSCO::remove()
{
    unlink_on_destruction_ = true;
}

const fs::path&
CachedSCO::path() const
{
    return path_;
}

void
CachedSCO::checkMountPointOnline_() const
{
    if (mntPoint_->isOffline())
    {
        throw fungi::IOException("mountpoint is offline",
                                 mntPoint_->getPath().string().c_str());
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
