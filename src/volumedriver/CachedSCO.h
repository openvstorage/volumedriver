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

#ifndef CACHED_SCO_H_
#define CACHED_SCO_H_

#include "SCO.h"
#include "Types.h"

#include <boost/filesystem.hpp>
#include <boost/intrusive/set.hpp>

#include <youtils/FileDescriptor.h>

namespace volumedriver
{

using youtils::FDMode;

class SCOCacheMountPoint;
class SCOCacheNamespace;

// Z42: redundant to the one in SCOCacheMountPoint.h - clean up!
typedef boost::intrusive_ptr<SCOCacheMountPoint> SCOCacheMountPointPtr;

class CachedSCO
    : public boost::intrusive::set_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink> >
{
    friend class SCOCacheMountPoint;
    friend class CachedSCOTest;
    friend class SCOCache;

public:
    ~CachedSCO();

    CachedSCO(const CachedSCO&) = delete;
    CachedSCO& operator=(const CachedSCO&) = delete;

    SCOCacheNamespace*
    getNamespace() const;

    SCO
    getSCO() const;

    SCOCacheMountPointPtr
    getMountPoint();

    uint64_t
    getSize() const;

    uint64_t
    getRealSize() const;

    float
    getXVal() const;

    void
    setXVal(float);

    OpenSCOPtr
    open(FDMode mode);

    void
    incRefCount(uint32_t num);

    uint32_t
    use_count();

    void
    remove();

    const boost::filesystem::path&
    path() const;

private:
    DECLARE_LOGGER("CachedSCO");

    const boost::filesystem::path path_;
    SCOCacheNamespace* const nspace_;
    SCO scoName_;
    SCOCacheMountPointPtr mntPoint_;
    uint64_t size_;
    float xVal_;
    bool disposable_;
    bool unlink_on_destruction_;
    std::atomic<uint32_t> refcnt_;

    // protect the following four from being called arbitrarily in the code:
    //
    // - scans an existing SCO, only allowed from SCOCacheMountPoint
    CachedSCO(SCOCacheNamespace* nspace,
              SCO scoName,
              SCOCacheMountPointPtr mntPoint,
              const boost::filesystem::path& path);

    // - create a new SCO, only allowed from SCOCacheMountPoint
    CachedSCO(SCOCacheNamespace* nspace,
              SCO scoName,
              SCOCacheMountPointPtr mntPoint,
              uint64_t maxSize,
              float xval);

    // the following 2 need to be protected against races by routing the
    // calls through SCOCache and relying on the locking there. Possible races:
    //
    // - the cleaner (read, periodic action thread) vs. datastore (write,
    //   threadpool thread)
    // - datastore (read, volume thread) vs. datastore (write, threadpool thread)
    //
    // The latter one is actually already prevented by the datastore locking,
    // so isDisposable() could be made public, but this is obviously too brittle.
    void
    setDisposable();

    bool
    isDisposable() const;

    void
    checkMountPointOnline_() const;

    friend void
    intrusive_ptr_add_ref(CachedSCO *);

    friend void
    intrusive_ptr_release(CachedSCO *);
};

}

#endif // !CACHED_SCO_H_

// Local Variables: **
// mode: c++ **
// End: **
