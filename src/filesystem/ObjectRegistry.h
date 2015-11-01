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

#ifndef VFS_OBJECT_REGISTRY_H_
#define VFS_OBJECT_REGISTRY_H_

#include "ClusterId.h"
#include "FileSystemParameters.h"
#include "LockedArakoon.h"
#include "NodeId.h"
#include "ObjectRegistration.h"
#include "OwnerTagAllocator.h"

#include <list>
#include <mutex>

#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/exception/all.hpp>

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
                   std::shared_ptr<LockedArakoon> larakoon);

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

    // use a vector instead?
    std::list<ObjectId>
    list();

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

private:
    DECLARE_LOGGER("ObjectRegistry");

    const ClusterId cluster_id_;
    const NodeId node_id_;
    std::shared_ptr<LockedArakoon> larakoon_;
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
                  LockedArakoon::PrepareSequenceFun&& prep_fun,
                  RetryOnArakoonAssert retry_on_assert);

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
