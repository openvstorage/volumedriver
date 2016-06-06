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

#ifndef SCO_CACHE_MOUNT_POINT_H_
#define SCO_CACHE_MOUNT_POINT_H_

#include "MountPointConfig.h"

#include <list>

#include <boost/filesystem.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/serialization/utility.hpp>

#include <youtils/Logging.h>

#include "SCOCacheInfo.h"
#include <youtils/SpinLock.h>
#include "Types.h"

namespace volumedriver
{
namespace fs = boost::filesystem;
namespace bu = boost::uuids;

class SCOCache;
class SCOCacheNamespace;

class SCOCacheMountPoint

{
public:

    SCOCacheMountPoint(SCOCache& scoCache,
                       const MountPointConfig& cfg,
                       bool restart);

    SCOCacheMountPoint(const SCOCacheMountPoint&) = delete;
    SCOCacheMountPoint& operator=(const SCOCacheMountPoint&) = delete;

    ~SCOCacheMountPoint();

    void
    newMountPointStage2(uint64_t errcount);

    const bu::uuid&
    uuid() const;

    void
    setErrorCount(uint64_t errcount);

    uint64_t
    getErrorCount() const;

    const fs::path&
    getPath() const;

    uint64_t
    getCapacity() const;

    uint64_t
    getUsedSize() const;

    uint64_t
    getFreeDiskSpace() const;

    void
    updateUsedSize(int64_t diff);

    // Z42: use SCOCacheNamespace isof std::string throughout the xNamespace()
    // methods
    void
    addNamespace(const backend::Namespace& nsname);

    void
    removeNamespace(const backend::Namespace& nsname);

    bool
    hasNamespace(const backend::Namespace& nsname);

    void
    scanNamespace(SCOCacheNamespace* nspace);

    void
    setOffline();

    bool
    isOffline() const;

    void
    setChoking(uint32_t choking);

    void
    setNoChoking();

    bool
    isChoking() const;

    SCOCache&
    getSCOCache() const;

    static bool
    exists(const MountPointConfig& cfg);

    boost::optional<uint32_t> const
    getThrottleUsecs() {
        return choking_;
    }

private:
    friend class ErrorHandlingTest;
    friend class boost::serialization::access;

    DECLARE_LOGGER("SCOCacheMountPoint");

    mutable fungi::SpinLock usedLock_;
    SCOCache& scoCache_;
    const fs::path path_;
    uint64_t capacity_;
    uint64_t used_;
    std::atomic<uint32_t> refcnt_;
    boost::optional<uint32_t> choking_;
    bool offline_;
    bu::uuid uuid_;
    uint64_t errcount_;
    bool initialised_;

    static const std::string lockfile_;

    void
    restart_();

    void
    scan_();

    void
    updateUsedSize_(int64_t diff);

    void
    newMountPointStage1_();

    static bool
    validateNamespace_(const backend::Namespace& nsname);

    static void
    validateNamespace_throw_(const backend::Namespace& nsname);

    bool
    empty_();

    void
    validateParams_();

    friend void
    intrusive_ptr_add_ref(SCOCacheMountPoint*);

    friend void
    intrusive_ptr_release(SCOCacheMountPoint*);

    fs::path
    lockFilePath_() const;

    void
    readMetaData_();

    void
    writeMetaData_() const;

    template<class Archive>
    void serialize(Archive& ar,
                   const unsigned int version);
};

}

#endif // !SCO_CACHE_MOUNT_POINT_H_

// Local Variables: **
// mode: c++ **
// End: **
