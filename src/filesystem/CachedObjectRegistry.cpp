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

#include "CachedObjectRegistry.h"

#include <algorithm>
#include <iterator>

namespace volumedriverfs
{

#define LOCK()                                 \
    boost::lock_guard<decltype(lock_)> lg__(lock_)

namespace be = backend;
namespace bpt = boost::property_tree;
namespace vd = volumedriver;
namespace yt = youtils;

// General remark: it's ok / expected that the cache is not at all times in sync
// with the backend.
// We'll make use of this by keeping the locked sections small.
CachedObjectRegistry::CachedObjectRegistry(const ClusterId& cluster_id,
                                           const NodeId& node_id,
                                           std::shared_ptr<yt::LockedArakoon> larakoon,
                                           size_t cache_capacity)
    : registry_(cluster_id, node_id, larakoon)
    , cache_("ObjectRegistryCache",
             cache_capacity)
{
    list(RefreshCache::T);
}

ObjectRegistrationPtr
CachedObjectRegistry::register_base_volume(const ObjectId& vol_id,
                                           const be::Namespace& nspace,
                                           FailOverCacheConfigMode foccmode)
{
    return update_cache_<decltype(nspace),
                         decltype(foccmode)>(&ObjectRegistry::register_base_volume,
                                             vol_id,
                                             nspace,
                                             foccmode);
}

ObjectRegistrationPtr
CachedObjectRegistry::register_file(const ObjectId& id)
{
    return update_cache_(&ObjectRegistry::register_file,
                         id);
}

ObjectRegistrationPtr
CachedObjectRegistry::register_clone(const ObjectId& clone_id,
                                     const be::Namespace& clone_nspace,
                                     const ObjectId& parent_id,
                                     const MaybeSnapshotName& maybe_parent_snap,
                                     FailOverCacheConfigMode foccmode)
{
    // We don't update the parent registration here. It has changed as it has a new
    // descendant but that shouldn't affect the rerouting.
    // This might point to the fact that the cache should only be a
    // map<VolumeID, NodeID>
    // instead of caching the ObjectRegistration, as using info from it except from
    // the NodeID is suspicious.
    return update_cache_<decltype(clone_nspace),
                         decltype(parent_id),
                         decltype(maybe_parent_snap),
                         decltype(foccmode)>(&ObjectRegistry::register_clone,
                                             clone_id,
                                             clone_nspace,
                                             parent_id,
                                             maybe_parent_snap,
                                             foccmode);
}

void
CachedObjectRegistry::unregister(const ObjectId& vol_id)
{
    try
    {
        registry_.unregister(vol_id);

        LOCK();
        cache_.erase(vol_id);
    }
    catch (ObjectNotRegisteredException&)
    {
        LOCK();
        cache_.erase(vol_id);
        throw;
    }
}

ObjectRegistrationPtr
CachedObjectRegistry::find(const ObjectId& vol_id,
                           IgnoreCache ignore_cache)
{
    if (ignore_cache == IgnoreCache::F)
    {
        LOCK();
        auto res(cache_.find(vol_id));
        if (res)
        {
            return *res;
        }
    }

    ObjectRegistrationPtr reg(registry_.find(vol_id));

    LOCK();

    if (reg != nullptr)
    {
        // we don't care if someone put it there in the mean time.
        cache_.insert(vol_id,
                      reg);
    }
    else
    {
        cache_.erase(vol_id);
    }

    return reg;
}

ObjectRegistrationPtr
CachedObjectRegistry::find_throw(const ObjectId& vol_id,
                                 IgnoreCache ignore_cache)
{
    auto reg(find(vol_id, ignore_cache));
    if (reg == nullptr)
    {
        throw ObjectNotRegisteredException() << error_object_id(vol_id);
    }

    return reg;
}

void
CachedObjectRegistry::drop_cache()
{
    LOCK();
    cache_.clear();
}

std::list<ObjectId>
CachedObjectRegistry::list(RefreshCache refresh_cache)
{
    std::list<ObjectId> objs(registry_.list());

    if (refresh_cache == RefreshCache::T)
    {
        for (const auto& id : objs)
        {
            ObjectRegistrationPtr reg(registry_.find(id));
            if (reg != nullptr)
            {
                LOCK();
                cache_.insert(id,
                              reg);
            }
        }
    }

    return objs;
}

template<typename... A>
ObjectRegistrationPtr
CachedObjectRegistry::update_cache_(ObjectRegistrationPtr
                                    (ObjectRegistry::*fun)(const ObjectId&,
                                                           A... args),
                                    const ObjectId& id,
                                    A... args)
{
    ObjectRegistrationPtr reg;

    try
    {
        reg = (registry_.*fun)(id, args...);
        VERIFY(reg);
    }
    catch (ObjectNotRegisteredException& e)
    {
        LOCK();
        cache_.erase(id);
        throw;
    }

    LOCK();
    cache_.insert(id,
                  reg);

    return reg;
}

ObjectRegistrationPtr
CachedObjectRegistry::prepare_migrate(arakoon::sequence& seq,
                                      const ObjectId& vol_id,
                                      const NodeId& from,
                                      const NodeId& to)
{
    return registry_.prepare_migrate(seq, vol_id, from, to);
}

ObjectRegistrationPtr
CachedObjectRegistry::migrate(const ObjectId& vol_id,
                              const NodeId& from,
                              const NodeId& to)
{
    return update_cache_<decltype(from),
                         decltype(to)>(&ObjectRegistry::migrate,
                                       vol_id,
                                       from,
                                       to);
}

void
CachedObjectRegistry::drop_entry_from_cache(const ObjectId& id)
{
    LOCK();
    cache_.erase(id);
}

void
CachedObjectRegistry::TESTONLY_add_to_cache_(ObjectRegistrationPtr reg)
{
    LOCK();
    cache_.insert(reg->volume_id,
                  reg);
}

void
CachedObjectRegistry::set_volume_as_template(const ObjectId& vol_id)
{
    update_cache_(&ObjectRegistry::set_volume_as_template,
                  vol_id);
}

void
CachedObjectRegistry::TESTONLY_add_to_registry_(const ObjectRegistration& reg)
{
    LOCK();
    registry_.TESTONLY_add_to_registry(reg);
}

ObjectRegistrationPtr
CachedObjectRegistry::convert_base_to_clone(const ObjectId& clone_id,
                                            const be::Namespace& clone_nspace,
                                            const ObjectId& parent_id,
                                            const MaybeSnapshotName& maybe_parent_snap,
                                            FailOverCacheConfigMode foccmode)
{
    return update_cache_<decltype(clone_nspace),
                         decltype(parent_id),
                         decltype(maybe_parent_snap)>(&ObjectRegistry::convert_base_to_clone,
                                                      clone_id,
                                                      clone_nspace,
                                                      parent_id,
                                                      maybe_parent_snap,
                                                      foccmode);
}

void
CachedObjectRegistry::set_foc_config_mode(const ObjectId& id,
                                          FailOverCacheConfigMode foc_cm)
{
    update_cache_<decltype(foc_cm)>(&ObjectRegistry::set_foc_config_mode,
                                    id,
                                    foc_cm);
}

}
