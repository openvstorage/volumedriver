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

#include "CachedSCO.h"
#include "SCOCache.h"
#include "SCOCacheMountPoint.h"
#include "SCOCacheNamespace.h"

#include <boost/serialization/string.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include <youtils/IOException.h>

#include <youtils/FileUtils.h>
#include <youtils/FileDescriptor.h>
#include <youtils/Serialization.h>
#include <youtils/UUID.h>

namespace volumedriver
{

namespace yt = youtils;

#define USED_LOCK()                             \
    fungi::ScopedSpinLock __l(usedLock_)

void
intrusive_ptr_add_ref(SCOCacheMountPoint* mp)
{
    ASSERT(mp);
    ++mp->refcnt_;
}

void
intrusive_ptr_release(SCOCacheMountPoint* mp)
{
    if (mp and --mp->refcnt_ == 0)
    {
        delete mp;
    }
}

const std::string SCOCacheMountPoint::lockfile_ = ".scocache";

bool
SCOCacheMountPoint::exists(const MountPointConfig& cfg)
{
    return fs::exists(cfg.path / lockfile_);
}

SCOCacheMountPoint::SCOCacheMountPoint(SCOCache& scoCache,
                                       const MountPointConfig& cfg,
                                       bool restart)
    : usedLock_()
    , scoCache_(scoCache)
    , path_(cfg.path)
    , garbage_path_(path_ / ".garbage")
    , capacity_(cfg.size)
    , used_(0)
    , refcnt_(0)
    , offline_(false)
    , errcount_(0)
    , initialised_(false)
{
    if (restart)
    {
        restart_();
    }
    else
    {
        newMountPointStage1_();
    }

    deferred_file_remover_ = std::make_unique<yt::DeferredFileRemover>(garbage_path_);
}

SCOCacheMountPoint::~SCOCacheMountPoint()
{
    LOG_DEBUG(path_ << ": destroying");
}

fs::path
SCOCacheMountPoint::lockFilePath_() const
{
    return path_ / lockfile_;
}

template<class Archive> void
SCOCacheMountPoint::serialize(Archive& ar,
                              const unsigned int /* version */)
{
    for (bu::uuid::value_type& u : uuid_)
    {
        ar & u;
    }

    ar & errcount_;
}

void
SCOCacheMountPoint::readMetaData_()
{
    fs::ifstream ifs(lockFilePath_());
    boost::archive::binary_iarchive ia(ifs);
    ia >> *this;
    ifs.close();
}

void
SCOCacheMountPoint::writeMetaData_() const
{
    youtils::Serialization::serializeAndFlush<boost::archive::binary_oarchive>(lockFilePath_(),
                                                                      *this);
}

void
SCOCacheMountPoint::scan_()
{
    fs::recursive_directory_iterator end;

    for (fs::recursive_directory_iterator it(path_); it != end; ++it)
    {
        const auto st = it->status();
        if (not fs::is_regular_file(st))
        {
            if (not fs::is_directory(st))
            {
                LOG_WARN(path_ << ": found non-file entry " << it->path());
            }
        }
        else
        {
            if (not SCO::isSCOString(it->path().filename().string()))
            {
                LOG_WARN(path_ << ": ignoring non-SCO entry " << it->path());
            }
            else
            {
                const size_t size = fs::file_size(it->path());
                LOG_DEBUG(path_ << ": adding " << it->path() << ", size " << size);
                updateUsedSize_(size);
            }
        }
    }
}

void
SCOCacheMountPoint::restart_()
{
    LOG_TRACE(path_);

    validateParams_();
    readMetaData_();
    scan_();

    initialised_ = true;

    LOG_DEBUG(path_ << ": restarted");
}

void
SCOCacheMountPoint::newMountPointStage1_()
{
    LOG_TRACE(path_);

    if (!fs::exists(path_))
    {
        throw fungi::IOException("MountPoint does not exist",
                                 path_.string().c_str(),
                                 ENOENT);
    }
    else
    {
        if (!empty_())
        {
            throw fungi::IOException("MountPoint not empty",
                                     path_.string().c_str(),
                                     EEXIST);
        }
    }

    validateParams_();
    bu::basic_random_generator<boost::mt19937> uuidgen;
    uuid_ = uuidgen();

    LOG_DEBUG(path_ << ": creation stage 1 succeeded");
}

void
SCOCacheMountPoint::newMountPointStage2(uint64_t errcount)
{
    VERIFY(!initialised_);
    errcount_ = errcount;
    writeMetaData_();
    initialised_ = true;
    LOG_DEBUG(path_ << ": creation stage 2 succeeded");
}

const fs::path&
SCOCacheMountPoint::getPath() const
{
    return path_;
}

void
SCOCacheMountPoint::validateParams_()
{
    if (path_.leaf() == "lost+found")
    {
        LOG_ERROR(path_ << ": invalid path");
        throw fungi::IOException("invalid path specified",
                                 path_.string().c_str(),
                                 EINVAL);
    }

    if (capacity_ == 0)
    {
        LOG_ERROR(path_ << ": invalid capacity 0 specified");
        throw fungi::IOException("invalid capacity specified",
                                 path_.string().c_str(),
                                 EINVAL);
    }

    // Z42: do away with this adjustment, possibly renaming capacity_ altogether
    uint64_t fsSize = FileUtils::filesystem_size(path_);

    if (capacity_ == std::numeric_limits<uint64_t>::max())
    {
        capacity_ = fsSize;
    }

    else if (fsSize < capacity_)
    {
        LOG_ERROR("" << path_ << ": filesystem cannot hold specified capacity: "
                  << fsSize << " < " << capacity_);
        throw fungi::IOException("filesystem cannot hold specified capacity",
                                 path_.string().c_str(),
                                 ENOSPC);
    }
}

void
SCOCacheMountPoint::scanNamespace(SCOCacheNamespace* nspace)
{
    VERIFY(initialised_);

    const fs::path p(path_ / nspace->getName().str());

    LOG_DEBUG(path_ << ": scanning mountpoint for namespace " <<
              nspace->getName() << "( " << p << ")");

    THROW_UNLESS(hasNamespace(nspace->getName()));

    fs::directory_iterator end;

    for (fs::directory_iterator it(p); it != end; ++it)
    {
       if (!fs::is_regular_file(it->status()))
       {
           LOG_WARN("ignoring non-file entry " << it->path());
           continue;
       }

       const std::string fname(it->path().filename().string());
       if (!SCO::isSCOString(fname))
       {
           LOG_WARN("ignoring non-SCO entry " << it->path());
           continue;
       }

       SCO scoName(fname);

       LOG_DEBUG(path_ << ": found SCO " << fname << " in " << p);

       CachedSCOPtr sco(new CachedSCO(nspace,
                                      scoName,
                                      this,
                                      it->path()));

       scoCache_.insertScannedSCO(sco);
   }

    LOG_DEBUG(path_ << ": namespace " << nspace->getName() << " scanned");
}

// Y42: Not needed since lost+found is not a valid namespace name
bool
SCOCacheMountPoint::validateNamespace_(const backend::Namespace& nspace)
{
    return (nspace.str() != "lost+found");
}

void
SCOCacheMountPoint::validateNamespace_throw_(const backend::Namespace& nspace)
{
    if (!validateNamespace_(nspace))
    {
        LOG_ERROR("invalid namespace " << nspace);
        throw fungi::IOException("invalid namespace",
                                 nspace.c_str());
    }
}

void
SCOCacheMountPoint::addNamespace(const backend::Namespace& nspace)
{
    VERIFY(initialised_);

    LOG_INFO(path_ << ": adding namespace " << nspace << " to mountpoint");

    validateNamespace_throw_(nspace);

    if (hasNamespace(nspace))
    {
        // be more forgiving?
        LOG_ERROR(path_ << " namespace " << nspace << " already exists");
        throw fungi::IOException("namespace already exists",
                                 nspace.c_str(),
                                 EEXIST);
    }

    try
    {
        fs::create_directories(path_ / nspace.str());
    }
    catch(std::exception&)
    {
        LOG_ERROR("Problem adding namespace " << nspace << " to mountpoint " <<
                  getPath());
    }
}

void
SCOCacheMountPoint::removeNamespace(const backend::Namespace& nspace)
{
    VERIFY(initialised_);

    LOG_INFO(path_ << ": removing namespace " << nspace << " from mountpoint");

    if (!hasNamespace(nspace))
    {
        // be more forgiving?
        LOG_ERROR(path_ << " namespace " << nspace << " does not exist");
        throw fungi::IOException("namespace does not exist",
                                 nspace.c_str(),
                                 ENOENT);
    }

    validateNamespace_throw_(nspace);
    fs::remove_all(path_ / nspace.str());
}

bool
SCOCacheMountPoint::hasNamespace(const backend::Namespace& nspace)
{
    VERIFY(initialised_);
    return fs::exists(path_ / nspace.str());
}

void
SCOCacheMountPoint::setOffline()
{
    VERIFY(initialised_);
    offline_ = true;
}

bool
SCOCacheMountPoint::isOffline() const
{
    VERIFY(initialised_);
    return offline_;
}

void
SCOCacheMountPoint::setChoking(uint32_t tm)
{
    VERIFY(initialised_);
    choking_.reset(tm);
}

void
SCOCacheMountPoint::setNoChoking()
{
    VERIFY(initialised_);
    choking_.reset();
}


bool
SCOCacheMountPoint::isChoking() const
{
    VERIFY(initialised_);
    return (choking_ != boost::none);
}

uint64_t
SCOCacheMountPoint::getCapacity() const
{
    return capacity_;
}

uint64_t
SCOCacheMountPoint::getUsedSize() const
{
    VERIFY(initialised_);
    USED_LOCK();
    return used_;
}

uint64_t
SCOCacheMountPoint::getFreeDiskSpace() const
{
    return FileUtils::filesystem_free_size(path_);
}

void
SCOCacheMountPoint::updateUsedSize(int64_t diff)
{
    VERIFY(initialised_);
    USED_LOCK();
    updateUsedSize_(diff);
}

void
SCOCacheMountPoint::updateUsedSize_(int64_t diff)
{
    if (diff >= 0)
    {
        VERIFY(used_ + diff <= capacity_);
        used_ += diff;
    }
    else if (diff < 0)
    {
        diff *= -1;
        VERIFY(static_cast<uint64_t>(diff) <= used_);
        used_ -= diff;
    }
}

SCOCache&
SCOCacheMountPoint::getSCOCache() const
{
    return scoCache_;
}

bool
SCOCacheMountPoint::empty_()
{
    VERIFY(fs::exists(path_));

    if (fs::exists(lockFilePath_()))
    {
        return false;
    }

    fs::directory_iterator end;

    for (fs::directory_iterator it(path_); it != end; ++it)
    {
        if (it->path() == garbage_path_)
        {
            continue;
        }
        else if (validateNamespace_(Namespace(it->path().filename().string())))
        {
            return false;
        }
    }

    return true;
}

uint64_t
SCOCacheMountPoint::getErrorCount() const
{
    VERIFY(initialised_);
    return errcount_;
}

void
SCOCacheMountPoint::setErrorCount(uint64_t errcount)
{
    VERIFY(initialised_);
    errcount_ = errcount;
    writeMetaData_();
}

const bu::uuid&
SCOCacheMountPoint::uuid() const
{
    VERIFY(initialised_);
    return uuid_;
}

void
SCOCacheMountPoint::addToGarbage(const fs::path& p)
{
    VERIFY(deferred_file_remover_ != nullptr);
    deferred_file_remover_->schedule_removal(p);
}

}

// Local Variables: **
// mode: c++ **
// End: **
