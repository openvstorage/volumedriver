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

#ifndef VFS_OBJECT_REGISTRY_H_
#define VFS_OBJECT_REGISTRY_H_

#include "ClusterId.h"
#include "FileSystemParameters.h"
#include "NodeId.h"
#include "ObjectRegistration.h"
#include "OwnerTagAllocator.h"

#include <list>
#include <mutex>

#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/exception/all.hpp>

#include <youtils/LockedArakoon.h>
#include <youtils/Logging.h>

namespace volumedriverfs
{

BOOLEAN_ENUM(ConvertBaseToClone);

struct ObjectRegistryException : virtual std::exception, virtual boost::exception { };
struct ObjectAlreadyRegisteredException : virtual ObjectRegistryException { };
struct ObjectNotRegisteredException : virtual ObjectRegistryException { };
struct InvalidOperationException : virtual ObjectRegistryException { };
struct ConflictingUpdateException : virtual ObjectRegistryException { };
struct WrongOwnerException : virtual ObjectRegistryException { };
struct InconsistencyException : virtual ObjectRegistryException { };

typedef boost::error_info<struct tag_volume_id, ObjectId> error_object_id;
typedef boost::error_info<struct tag_parent_volume_id, ObjectId> error_parent_volume_id;
typedef boost::error_info<struct tag_desc, std::string> error_desc;
typedef boost::error_info<struct tag_node_id, NodeId> error_node_id;
typedef boost::error_info<struct tag_other_node_id, NodeId> error_other_node_id;

class ObjectRegistry
{
public:
    ObjectRegistry(const ClusterId& cluster_id,
                   const NodeId& node_id,
                   std::shared_ptr<youtils::LockedArakoon> larakoon);

    ~ObjectRegistry() = default;

    ObjectRegistry(const ObjectRegistry&) = delete;

    ObjectRegistry&
    operator=(const ObjectRegistry&) = delete;

    void
    destroy();

    ObjectRegistrationPtr
    register_base_volume(const ObjectId&_id,
                         const backend::Namespace&,
                         FailOverCacheConfigMode = FailOverCacheConfigMode::Automatic);

    ObjectRegistrationPtr
    register_file(const ObjectId& vol_id);

    //this call:
    //    - adds a new registration for the clone
    //    - adds the cloneID to the parent template descendants list
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
                          const FailOverCacheConfigMode = FailOverCacheConfigMode::Automatic);

    void
    unregister(const ObjectId& vol_id);

    void
    wipe_out(const ObjectId& oid);

    ObjectRegistrationPtr
    find_owned_throw(const ObjectId& vol_id);

    ObjectRegistrationPtr
    find_throw(const ObjectId& vol_id);

    ObjectRegistrationPtr
    find(const ObjectId& vol_id);

    std::vector<ObjectId>
    list();

    std::vector<ObjectRegistrationPtr>
    get_all_registrations(const size_t batch_size = 1024);

    ObjectRegistrationPtr
    prepare_migrate(arakoon::sequence& seq,
                    const ObjectId& vol_id,
                    const NodeId& from,
                    const NodeId& to);

    ObjectRegistrationPtr
    migrate(const ObjectId& vol_id,
            const NodeId& from,
            const NodeId& to);

    const ClusterId
    cluster_id() const
    {
        return cluster_id_;
    }

    const NodeId
    node_id() const
    {
        return node_id_;
    }

    ObjectRegistrationPtr
    set_volume_as_template(const ObjectId& vol_id);

    void
    TESTONLY_add_to_registry(const ObjectRegistration& reg);

    std::string
    prefix() const;

    ObjectRegistrationPtr
    set_foc_config_mode(const ObjectId&,
                        FailOverCacheConfigMode foc_cm);

    std::shared_ptr<youtils::LockedArakoon>
    locked_arakoon()
    {
        return larakoon_;
    }

private:
    DECLARE_LOGGER("ObjectRegistry");

    const ClusterId cluster_id_;
    const NodeId node_id_;
    std::shared_ptr<youtils::LockedArakoon> larakoon_;
    OwnerTagAllocator owner_tag_allocator_;

    std::string
    make_key_(const ObjectId& vol_id) const;

    ObjectRegistrationPtr
    find_(const std::string& key,
                 arakoon::buffer& buf);

    ObjectRegistrationPtr
    find_throw_(const ObjectId& vol_id,
                       const std::string& key,
                       arakoon::buffer& buf);

    void
    with_owned_volume_(const ObjectId& id,
                       const NodeId& owner,
                       std::function<void(const std::string& key,
                                          const arakoon::buffer& buf,
                                          const ObjectRegistration& reg)>&& fn);

    void
    run_sequence_(const ObjectId& id,
                  const char* desc,
                  youtils::LockedArakoon::PrepareSequenceFun prep_fun,
                  youtils::RetryOnArakoonAssert retry_on_assert);

    ObjectRegistrationPtr
    prepare_register_base_or_file_(arakoon::sequence&,
                                   const ObjectId&,
                                   const backend::Namespace&,
                                   const ObjectType,
                                   const FailOverCacheConfigMode);

    ObjectRegistrationPtr
    register_base_or_file_(const ObjectId&_id,
                           const backend::Namespace&,
                           const ObjectType,
                           const FailOverCacheConfigMode);

    ObjectRegistrationPtr
    prepare_register_clone_(arakoon::sequence& seq,
                            const ObjectId& clone_id,
                            const backend::Namespace& clone_nspace,
                            const ObjectId& parent_id,
                            const MaybeSnapshotName& maybe_parent_snap,
                            const ConvertBaseToClone& convert_base_to_clone,
                            const FailOverCacheConfigMode);

    void
    do_prepare_unregister_volumoid_(const ObjectId& id,
                                    const std::string& key,
                                    const arakoon::buffer& old_buf,
                                    const ObjectRegistration& old_reg,
                                    arakoon::sequence& seq);

    void
    prepare_unregister_volumoid_(arakoon::sequence& seq,
                                 const ObjectId& id);

    ObjectRegistrationPtr
    do_prepare_set_volume_as_template_(const std::string& key,
                                       const arakoon::buffer& old_buf,
                                       const ObjectRegistration& old_reg,
                                       arakoon::sequence& seq);

    ObjectRegistrationPtr
    prepare_set_volume_as_template_(arakoon::sequence& seq,
                                    const ObjectId& id);

    void
    prepare_unregister_file_(arakoon::sequence& seq,
                             const ObjectId& id);

    ObjectRegistrationPtr
    maybe_upgrade_(ObjectRegistrationPtr reg);

    ObjectRegistrationPtr
    do_prepare_set_foc_config_mode_(const std::string& key,
                                    const arakoon::buffer& old_buf,
                                    const ObjectRegistration& old_reg,
                                    const FailOverCacheConfigMode foc_cm,
                                    arakoon::sequence& seq);

    ObjectRegistrationPtr
    prepare_set_foc_config_mode_(arakoon::sequence& seq,
                                 const ObjectId& id,
                                 const FailOverCacheConfigMode foc_cm);
};

}

#endif // !VFS_OBJECT_REGISTRY_H_
