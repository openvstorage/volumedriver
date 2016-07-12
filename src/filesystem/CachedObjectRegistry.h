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

#ifndef VFS_CACHED_OBJECT_REGISTRY_H_
#define VFS_CACHED_OBJECT_REGISTRY_H_

#include "ClusterId.h"
#include "NodeId.h"
#include "ObjectRegistration.h"
#include "ObjectRegistry.h"

#include <list>

#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/thread/mutex.hpp>

#include <youtils/BooleanEnum.h>
#include <youtils/Logging.h>
#include <youtils/LRUCacheToo.h>

namespace volumedriverfstest
{
class FileSystemTestBase;
class FileSystemTestSetup;
class RemoteTest;
}

namespace youtils
{
class LockedArakoon;
}

namespace volumedriverfs
{

BOOLEAN_ENUM(IgnoreCache);
BOOLEAN_ENUM(RefreshCache);

class CachedObjectRegistry
{
    friend class volumedriverfstest::FileSystemTestBase;
    friend class volumedriverfstest::FileSystemTestSetup;
    friend class volumedriverfstest::RemoteTest;

public:
    CachedObjectRegistry(const ClusterId& cluster_id,
                         const NodeId& node_id,
                         std::shared_ptr<youtils::LockedArakoon> registry,
                         size_t cache_capacity = 1024);

    ~CachedObjectRegistry() = default;

    CachedObjectRegistry(const CachedObjectRegistry&) = delete;

    CachedObjectRegistry&
    operator=(const CachedObjectRegistry&) = delete;

    ObjectRegistrationPtr
    register_base_volume(const ObjectId&,
                         const backend::Namespace&,
                         FailOverCacheConfigMode = FailOverCacheConfigMode::Automatic);

    ObjectRegistrationPtr
    register_clone(const ObjectId& clone_id,
                   const backend::Namespace& clone_nspace,
                   const ObjectId& parent_id,
                   const MaybeSnapshotName& maybe_parent_snap,
                   FailOverCacheConfigMode = FailOverCacheConfigMode::Automatic);

    ObjectRegistrationPtr
    convert_base_to_clone(const ObjectId& clone_id,
                          const backend::Namespace& clone_nspace,
                          const ObjectId& parent_id,
                          const MaybeSnapshotName& maybe_parent_snap,
                          FailOverCacheConfigMode = FailOverCacheConfigMode::Automatic);

    ObjectRegistrationPtr
    register_file(const ObjectId&);

    void
    unregister(const ObjectId&);

    ObjectRegistrationPtr
    find_throw(const ObjectId&,
               IgnoreCache);

    ObjectRegistrationPtr
    find(const ObjectId&,
         IgnoreCache);

    std::vector<ObjectId>
    list(RefreshCache = RefreshCache::F);

    ObjectRegistrationPtr
    prepare_migrate(arakoon::sequence& seq,
                    const ObjectId& vol_id,
                    const NodeId& from,
                    const NodeId& to);

    ObjectRegistrationPtr
    migrate(const ObjectId& vol_id,
            const NodeId& from,
            const NodeId& to);

    void
    drop_entry_from_cache(const ObjectId& vol_id);

    void
    drop_cache();

    const ClusterId
    cluster_id() const
    {
        return registry_.cluster_id();
    }

    const NodeId
    node_id() const
    {
        return registry_.node_id();
    }

    ObjectRegistry&
    registry()
    {
        return registry_;
    }

    void
    set_volume_as_template(const ObjectId& vol_id);

    void
    TESTONLY_add_to_registry_(const ObjectRegistration& reg);

    void
    set_foc_config_mode(const ObjectId&, FailOverCacheConfigMode);

private:
    DECLARE_LOGGER("CachedObjectRegistry");

    ObjectRegistry registry_;

    using Cache = youtils::LRUCacheToo<ObjectId,
                                       ObjectRegistrationPtr>;
    Cache cache_;

    // only protects the cache_, not the registry_
    boost::mutex lock_;

    void
    TESTONLY_remove_from_cache_(const ObjectId& id);

    void
    TESTONLY_add_to_cache_(ObjectRegistrationPtr reg);

    template<typename... A>
    ObjectRegistrationPtr
    update_cache_(ObjectRegistrationPtr
                  (ObjectRegistry::*fun)(const ObjectId&,
                                         A... args),
                  const ObjectId& id,
                  A... args);
};

}

#endif // !VFS_CACHED_OBJECT_REGISTRY_H_
