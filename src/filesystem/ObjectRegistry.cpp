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

#include "ObjectRegistry.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <youtils/ArakoonInterface.h>
#include <youtils/Assert.h>
#include <youtils/LockedArakoon.h>

namespace volumedriverfs
{

namespace ara = arakoon;
namespace be = backend;
namespace vd = volumedriver;
namespace yt = youtils;

#define ID()                                    \
    cluster_id_ << "/" << node_id_

namespace
{

typedef boost::archive::text_iarchive iarchive_type;
typedef boost::archive::text_oarchive oarchive_type;

// potentially extra copy but should be negligible as we're talking to arakoon
// and it makes the code much more readable
std::string
serialize_volume_registration(const ObjectRegistration& reg)
{
    std::stringstream ss;
    oarchive_type oa(ss);
    auto regp = &reg;
    oa << regp;
    return ss.str();
}

ObjectRegistrationPtr
deserialize_volume_registration(std::istream& is)
{
    ObjectRegistration* reg = nullptr;

    try
    {
        iarchive_type ia(is);
        ia >> reg;
        return ObjectRegistrationPtr(reg);
    }
    catch (...)
    {
        delete reg;
        throw;
    }
}

ObjectRegistrationPtr
deserialize_volume_registration(const ara::buffer& buf)
{
    using FunPtr = ObjectRegistrationPtr(*)(std::istream&);
    FunPtr fun = deserialize_volume_registration;
    return buf.as_istream<ObjectRegistrationPtr>(fun);
}

}

// TODO [bdv]
// Currently we support concurrent cloning and deletion of clones from the same
// parent as shown by the VolumeTest.concurrent_tree_actions. This works by
// - always using arakoon sequences with asserts if necessary
// - pre-checking that the assert will succeed if no concurrent update happens
// - (infinitely) retrying the whole action (inclusive the checking etc.) if we get an assertion failure
// Ideally we extend this behavior to all actions on the registry, more precisely, if
// actions A and B are done concurrently: "A || B" (might from from different nodes) then we exhibit the same behavior
//  as either "A then B"
//  or        "B then A"
// For example "setAsTemplate || unregister" will yield either:
//    "setAsTemplate then unregister" => no error on setAsTemplate and volume gone, or
//    "unregister then setAsTemplate" => volume gone, and setAsTemplate yields volumeNotRegisteredException
// -> currently the unregister might just fail with ConflictingUpdateException

ObjectRegistry::ObjectRegistry(const ClusterId& cluster_id,
                               const NodeId& node_id,
                               std::shared_ptr<yt::LockedArakoon> larakoon)
    : cluster_id_(cluster_id)
    , node_id_(node_id)
    , larakoon_(larakoon)
    , owner_tag_allocator_(cluster_id_,
                           larakoon_)
{
    VERIFY(larakoon_);
}

std::string
ObjectRegistry::prefix() const
{
    return cluster_id_.str() + "/registrations/";
}

void
ObjectRegistry::destroy()
{
    LOG_INFO("Removing object registrations for " << cluster_id_);

    std::vector<ObjectId> l(list());

    for (const auto& o : l)
    {
        ObjectRegistrationPtr reg(find(o));
        LOG_WARN(cluster_id_ << ": leaking " << reg->treeconfig.object_type << " " <<
                 reg->volume_id);
    }

    larakoon_->delete_prefix(prefix());
    owner_tag_allocator_.destroy();
}

std::string
ObjectRegistry::make_key_(const ObjectId& vol_id) const
{
    return prefix() + vol_id.str();
}

ObjectRegistrationPtr
ObjectRegistry::maybe_upgrade_(ObjectRegistrationPtr reg)
{
    if (reg->node_id == node_id() and
        reg->owner_tag == vd::OwnerTag(0))
    {
        LOG_INFO(reg->volume_id << ": old registration, upgrading it");

        const std::string key(make_key_(reg->volume_id));

        larakoon_->run_sequence("upgrade registration",
                                [&](ara::sequence& seq)
                                {
                                    ara::buffer buf(larakoon_->get(key));
                                    seq.add_assert(key,
                                                   buf);

                                    ObjectRegistrationPtr
                                        cur(deserialize_volume_registration(buf));

                                    if (cur->node_id == node_id() and
                                        cur->owner_tag == vd::OwnerTag(0))
                                    {
                                        ObjectRegistration new_reg(cur->getNS(),
                                                                   cur->volume_id,
                                                                   cur->node_id,
                                                                   cur->treeconfig,
                                                                   owner_tag_allocator_(),
                                                                   FailOverCacheConfigMode::Automatic);
                                        seq.add_set(key,
                                                    serialize_volume_registration(new_reg));
                                    }
                                },
                                yt::RetryOnArakoonAssert::T);
    }

    return reg;
}

ObjectRegistrationPtr
ObjectRegistry::find_(const std::string& key,
                      ara::buffer& buf)
{
    LOG_TRACE(ID() << ": key " << key);
    try
    {
        buf = larakoon_->get(key);
        ObjectRegistrationPtr reg(deserialize_volume_registration(buf));
        LOG_TRACE(ID() << ": found volume for key " << key);
        reg = maybe_upgrade_(reg);
        return reg;
    }
    catch (ara::error_not_found&)
    {
        LOG_TRACE(ID() << ": volume for key " <<
                  key << " not found");
        return nullptr;
    }
}

ObjectRegistrationPtr
ObjectRegistry::find_throw_(const ObjectId& vol_id,
                                   const std::string& key,
                                   ara::buffer& buf)
{
    ASSERT(make_key_(vol_id) == key);
    auto reg(find_(key, buf));
    if (reg == nullptr)
    {
        LOG_ERROR(ID() << ": volume " << vol_id << " not registered");
        throw ObjectNotRegisteredException() << error_object_id(vol_id);
    }

    return reg;
}

ObjectRegistrationPtr
ObjectRegistry::find(const ObjectId& vol_id)
{
    LOG_TRACE(ID() << ": looking up " << vol_id);

    const std::string key(make_key_(vol_id));
    ara::buffer buf;

    auto reg(find_(key, buf));
    if (reg != nullptr)
    {
        VERIFY(vol_id == reg->volume_id);
    }

    return reg;
}

ObjectRegistrationPtr
ObjectRegistry::find_throw(const ObjectId& vol_id)
{
    auto reg(find(vol_id));
    if (reg == nullptr)
    {
        LOG_ERROR(ID() << ": volume " << vol_id << " not registered");
        throw ObjectNotRegisteredException() << error_object_id(vol_id);
    }
    else
    {
        THROW_UNLESS(vol_id == reg->volume_id);
        return reg;
    }
}

std::vector<ObjectId>
ObjectRegistry::list()
{
    LOG_TRACE(ID() << ": getting list of registrations");

    const std::string pfx(prefix());

    // XXX:
    // * do we want to limit the number of max_elements returned?
    // * the locked scope is too big, but ara::value_list doesn't have a copy assignment
    //   operator

    ara::value_list l(larakoon_->prefix(prefix()));

    std::vector<ObjectId> vols;
    vols.reserve(l.size());

    ara::arakoon_buffer val;
    ara::value_list::iterator it(l.begin());

    while (it.next(val))
    {
        VERIFY(val.first > pfx.size());

        std::string s(static_cast<const char*>(val.second) + pfx.size(),
                      val.first - pfx.size());
        vols.emplace_back(ObjectId(s));
    }

    return vols;
}

std::vector<ObjectRegistrationPtr>
ObjectRegistry::get_all_registrations(size_t batch_size)
{
    LOG_TRACE(ID() << ": getting all registrations");

    ASSERT(batch_size > 0);

    // TODO: This is inefficient as list() converts the ara::value_list into a
    // vector<string>, shaving off the prefix and below it's added back again.
    // However, for now convenience trumps and in the long run the use of 'prefix'
    // will also have to be reconsidered for a big number of keys.
    const std::vector<ObjectId> ids(list());

    std::vector<ObjectRegistrationPtr> vec;
    vec.reserve(ids.size());

    if (not ids.empty())
    {
        const std::string last(make_key_(ids.back()));

        for (size_t i = 0; i < ids.size(); i += batch_size)
        {
            const ara::key_value_list kvl(larakoon_->range_entries(make_key_(ids[i]),
                                                                   true,
                                                                   last,
                                                                   true,
                                                                   batch_size));

            ara::key_value_list::iterator it(kvl.begin());

            ara::arakoon_buffer key;
            ara::arakoon_buffer val;

            while (it.next(key,
                           val))
            {
                std::stringstream ss;
                ss << std::string(static_cast<const char*>(val.second),
                                  val.first);
                vec.emplace_back(deserialize_volume_registration(ss));
            }
        }
    }

    return vec;
}

void
ObjectRegistry::with_owned_volume_(const ObjectId& id,
                                   const NodeId& owner,
                                   std::function<void(const std::string& key,
                                                      const ara::buffer& buf,
                                                      const ObjectRegistration& reg)>&& fn)
{
    LOG_TRACE(ID() << ": id");

    const std::string key(make_key_(id));

    ara::buffer buf;
    auto reg(find_(key, buf));
    if (reg == nullptr)
    {
        LOG_ERROR(ID() << ": volume " << id << " is not registered");
        throw ObjectNotRegisteredException() << error_object_id(id);
    }

    THROW_UNLESS(id == reg->volume_id);

    if (reg->node_id != owner)
    {
        LOG_ERROR(ID() << ": volume " << id << " is not hosted by " << owner <<
                  " but by " << reg->node_id);
        throw WrongOwnerException() << error_object_id(id)
                                    << error_node_id(owner)
                                    << error_other_node_id(reg->node_id);
    }

    fn(key, buf, *reg);
}

void
ObjectRegistry::run_sequence_(const ObjectId& id,
                              const char* desc,
                              yt::LockedArakoon::PrepareSequenceFun prep_fun,
                              yt::RetryOnArakoonAssert retry_on_assert)
{
    LOG_TRACE(desc);

    try
    {
        larakoon_->run_sequence(desc,
                                std::move(prep_fun),
                                retry_on_assert);
    }
    catch (ara::error_assertion_failed&)
    {
        throw ConflictingUpdateException() <<
            error_object_id(id) <<
            error_desc(desc) <<
            error_node_id(node_id());
    }
}

ObjectRegistrationPtr
ObjectRegistry::prepare_register_base_or_file_(ara::sequence& seq,
                                               const ObjectId& id,
                                               const be::Namespace& nspace,
                                               const ObjectType typ,
                                               const FailOverCacheConfigMode foccmode)
{
    LOG_DEBUG(id);

    ObjectRegistrationPtr
        reg(boost::make_shared<ObjectRegistration>(nspace,
                                                   id,
                                                   node_id(),
                                                   typ == ObjectType::Volume ?
                                                   ObjectTreeConfig::makeBase() :
                                                   ObjectTreeConfig::makeFile(),
                                                   owner_tag_allocator_(),
                                                   foccmode));

    const std::string key(make_key_(id));
    seq.add_assert(key,
                   ara::None());
    seq.add_set(key,
                serialize_volume_registration(*reg));

    return reg;
}

ObjectRegistrationPtr
ObjectRegistry::register_base_or_file_(const ObjectId& vol_id,
                                       const be::Namespace& nspace,
                                       const ObjectType typ,
                                       const FailOverCacheConfigMode foccmode)
{
    LOG_TRACE(ID() << ": registering " << vol_id << ", namespace " << nspace << ", type " << typ << ", foc config mode " << foccmode);

    VERIFY(typ == ObjectType::Volume or
           typ == ObjectType::File);

    ObjectRegistrationPtr reg;

    try
    {
        run_sequence_(vol_id,
                      typ == ObjectType::Volume ?
                      "register basic volume" :
                      "register file",
                      [&](ara::sequence& seq)
                      {
                          reg = prepare_register_base_or_file_(seq,
                                                               vol_id,
                                                               nspace,
                                                               typ,
                                                               foccmode);
                      },
                      yt::RetryOnArakoonAssert::F);
    }
    catch (ConflictingUpdateException&)
    {
        LOG_ERROR(ID() << ": failed to register " << vol_id << " - already present!?");
        throw ObjectAlreadyRegisteredException() <<
            error_object_id(vol_id) <<
            error_desc("failed to register volume") <<
            error_node_id(node_id());
    }

    VERIFY(reg);
    return reg;
}

ObjectRegistrationPtr
ObjectRegistry::register_base_volume(const ObjectId& vol_id,
                                     const be::Namespace& nspace,
                                     FailOverCacheConfigMode foccmode)
{
    LOG_INFO(ID() << ": registering " << vol_id <<
             ", namespace " << nspace <<
             ", foc config mode " << foccmode);

    return register_base_or_file_(vol_id,
                                  nspace,
                                  ObjectType::Volume,
                                  foccmode);
}

ObjectRegistrationPtr
ObjectRegistry::register_file(const ObjectId& id)
{
    LOG_INFO(ID() << ": registering file " << id);

    TODO("AR: reconsider nspace in ObjectRegistration of files");
    return register_base_or_file_(id,
                                  be::Namespace(id.str()),
                                  ObjectType::File,
                                  FailOverCacheConfigMode::Automatic);
}

ObjectRegistrationPtr
ObjectRegistry::prepare_register_clone_(ara::sequence& seq,
                                        const ObjectId& clone_id,
                                        const be::Namespace& clone_nspace,
                                        const ObjectId& parent_id,
                                        const MaybeSnapshotName& maybe_parent_snap,
                                        const ConvertBaseToClone& convert_base_to_clone,
                                        const FailOverCacheConfigMode foccmode)
{
    LOG_DEBUG("clone id " << clone_id <<
              ", nspace " << clone_nspace <<
              ", parent " << parent_id <<
              ", parent snapshot " << maybe_parent_snap <<
              ", foc config mode " << foccmode);

    // 1. Some sanity checking on the parent
    // (as we get the old buffer from arakoon this
    // guarantees assert on parent won't fail if
    // no concurrent updates)
    const std::string parent_key(make_key_(parent_id));
    const std::string clone_key(make_key_(clone_id));

    ara::buffer old_parent_buf;

    ObjectRegistrationPtr old_parent_reg(find_throw_(parent_id,
                                                     parent_key,
                                                     old_parent_buf));

    const ObjectTreeConfig& old_treeconfig = old_parent_reg->treeconfig;

    switch (old_treeconfig.object_type)
    {
    case ObjectType::File:
        {
            LOG_ERROR(ID() << ", clone " << clone_id << ": cannot clone from file " <<
                      parent_id);
            throw InvalidOperationException() <<
                error_object_id(clone_id) <<
                error_parent_volume_id(parent_id) <<
                error_desc("cannot clone from file");
        }
    case ObjectType::Volume:
        {
            if (not maybe_parent_snap)
            {
                LOG_ERROR(ID() << ", clone " << clone_id << ": cannot clone from volume " <<
                          parent_id << " - snapshot to clone from is missing");
                throw InvalidOperationException() <<
                    error_object_id(clone_id) <<
                    error_parent_volume_id(parent_id) <<
                    error_desc("cannot clone from volume if snapshot is not specified");
            }
            break;
        }
    case ObjectType::Template:
        {
            // XXX: We could be more lenient here and allow the snapshot if it matches
            // the snapshot the template was created from.
            if (maybe_parent_snap)
            {
                LOG_ERROR(ID() << ", clone " << clone_id <<
                          ": cannot clone from template " <<
                          parent_id << " - as a snapshot to clone from is specified");
                throw InvalidOperationException() <<
                    error_object_id(clone_id) <<
                    error_parent_volume_id(parent_id) <<
                    error_desc("cannot clone from template if snapshot is specified");
            }
            break;
        }
    }

    ObjectTreeConfig::Descendants new_descendants(old_treeconfig.descendants);

    const auto res(new_descendants.insert(std::make_pair(clone_id, maybe_parent_snap)));
    if(not res.second)
    {
        LOG_ERROR("Clone " << clone_id << " is already registered with parent " <<
                  parent_id << ", parent snapshot " << maybe_parent_snap);
        throw ObjectAlreadyRegisteredException() <<
            error_object_id(clone_id) <<
            error_parent_volume_id(parent_id) <<
            error_desc("clone was already registered with parent");
    }

    // 2. Some sanity checking on the clone (non-existence):
    // guarantees assert in sequence won't fail if no
    // concurrent updates
    {
        ara::buffer buf;
        if (convert_base_to_clone == ConvertBaseToClone::F)
        {
            if (find_(clone_key, buf) != nullptr)
            {
                LOG_ERROR("Clone " << clone_id << " is already registered in registry");
                throw ObjectAlreadyRegisteredException() << error_object_id(clone_id);
            }
        }
        else
        {
            if (find_(clone_key, buf) == nullptr)
            {
                LOG_ERROR("Clone " << clone_id << " is not registered in registry");
                throw ObjectNotRegisteredException() << error_object_id(clone_id);
            }
        }
    }

    //3. defining the arakoon sequence
    const ObjectRegistration
        new_parent_reg(old_parent_reg->getNS(),
                       old_parent_reg->volume_id,
                       old_parent_reg->node_id,
                       ObjectTreeConfig::makeParent(old_treeconfig.object_type,
                                                    new_descendants,
                                                    old_treeconfig.parent_volume),
                       old_parent_reg->owner_tag,
                       old_parent_reg->foc_config_mode);

    const auto
        clone_reg(boost::make_shared<ObjectRegistration>(clone_nspace,
                                                         clone_id,
                                                         node_id_,
                                                         ObjectTreeConfig::makeClone(parent_id),
                                                         owner_tag_allocator_(),
                                                         foccmode));

    //delete old clone registration
    if (convert_base_to_clone == ConvertBaseToClone::T)
    {
        ara::buffer old_clone_buf;
        ObjectRegistrationPtr old_clone_reg(find_throw_(clone_id,
                                                        clone_key,
                                                        old_clone_buf));
        seq.add_assert(clone_key,
                       old_clone_buf);
        seq.add_delete(clone_key);
    }
    //atomic update of template and insertion of clone
    seq.add_assert(parent_key,
                   old_parent_buf);
    seq.add_set(parent_key,
                serialize_volume_registration(new_parent_reg));
    seq.add_assert(clone_key,
                   ara::None());
    seq.add_set(clone_key,
                serialize_volume_registration(*clone_reg));

    return clone_reg;
}

ObjectRegistrationPtr
ObjectRegistry::register_clone(const ObjectId& clone_id,
                               const be::Namespace& clone_nspace,
                               const ObjectId& parent_id,
                               const MaybeSnapshotName& maybe_parent_snap,
                               const FailOverCacheConfigMode foccmode)
{
    ObjectRegistrationPtr reg;

    run_sequence_(clone_id,
                  "register clone",
                  [&](ara::sequence& seq)
                  {
                      reg = prepare_register_clone_(seq,
                                                    clone_id,
                                                    clone_nspace,
                                                    parent_id,
                                                    maybe_parent_snap,
                                                    ConvertBaseToClone::F,
                                                    foccmode);
                  },
                  yt::RetryOnArakoonAssert::T);

    VERIFY(reg);
    return reg;
}

ObjectRegistrationPtr
ObjectRegistry::convert_base_to_clone(const ObjectId& clone_id,
                                      const be::Namespace& clone_nspace,
                                      const ObjectId& parent_id,
                                      const MaybeSnapshotName& maybe_parent_snap,
                                      const FailOverCacheConfigMode foccmode)
{
    ObjectRegistrationPtr reg;

    run_sequence_(clone_id,
                  "convert base to clone",
                  [&](ara::sequence& seq)
                  {
                    reg = prepare_register_clone_(seq,
                                                  clone_id,
                                                  clone_nspace,
                                                  parent_id,
                                                  maybe_parent_snap,
                                                  ConvertBaseToClone::T,
                                                  foccmode);
                  },
                  yt::RetryOnArakoonAssert::T);

    VERIFY(reg);
    return reg;
}

ObjectRegistrationPtr
ObjectRegistry::prepare_migrate(ara::sequence& seq,
                                const ObjectId& vol_id,
                                const NodeId& from,
                                const NodeId& to)
{
    LOG_INFO(ID() << ": trying to add migration of " << vol_id << " from " <<
             from << " to " << to << " to sequence");

    ObjectRegistrationPtr reg;

    with_owned_volume_(vol_id,
                       from,
                       [&](const std::string& key,
                           const ara::buffer& old_val,
                           const ObjectRegistration& old_reg)
                       {
                           reg = boost::make_shared<ObjectRegistration>(old_reg.getNS(),
                                                                        old_reg.volume_id,
                                                                        to,
                                                                        old_reg.treeconfig,
                                                                        owner_tag_allocator_(),
                                                                        old_reg.foc_config_mode);

                           seq.add_assert(key,
                                          old_val);
                           seq.add_set(key,
                                       serialize_volume_registration(*reg));
                       });

    VERIFY(reg);
    return reg;
}

ObjectRegistrationPtr
ObjectRegistry::migrate(const ObjectId& vol_id,
                        const NodeId& from,
                        const NodeId& to)
{
    LOG_INFO(ID() << ": trying to move " << vol_id << " from " << from << " to " << to);

    ObjectRegistrationPtr reg;

    try
    {
        run_sequence_(vol_id,
                      "migrate volume",
                      [&](ara::sequence& seq)
                      {
                          reg = prepare_migrate(seq,
                                                vol_id,
                                                from,
                                                to);
                      },
                      yt::RetryOnArakoonAssert::F);
    }
    catch (ConflictingUpdateException&)
    {
        LOG_ERROR(ID() << ": failed to move " << vol_id <<
                  " from " << from << " to " << to);

        throw ConflictingUpdateException() <<
            error_object_id(vol_id) <<
            error_desc("failed to migrate") <<
            error_node_id(from) <<
            error_other_node_id(to);
    }

    VERIFY(reg);
    return reg;
}

void
ObjectRegistry::do_prepare_unregister_volumoid_(const ObjectId& id,
                                                const std::string& key,
                                                const ara::buffer& old_buf,
                                                const ObjectRegistration& old_reg,
                                                ara::sequence& seq)
{
    LOG_DEBUG(ID() << ": unregistering " << id);

    const ObjectTreeConfig& old_treeconfig = old_reg.treeconfig;

    ASSERT(old_treeconfig.object_type == ObjectType::Volume or
           old_treeconfig.object_type == ObjectType::Template);

    if (not old_treeconfig.descendants.empty())
    {
        LOG_ERROR(ID() << ": cannot remove " << old_treeconfig.object_type << " " <<
                  id << " as it still has descendants");

        for (const auto& d : old_treeconfig.descendants)
        {
            LOG_ERROR(ID() << ": descendant " << d.first << "(snapshot: " << d.second <<
                      ") is still present");
        }

        throw InvalidOperationException() <<
            error_object_id(id) <<
            error_desc("cannot remove volume or template as it still has descendants");
    }

    if (old_treeconfig.parent_volume)
    {
        // We want to support concurrent actions on sibling clones which affect
        // the parent volume registration. Therefore the two asserts in the sequence
        // should succeed in case of no concurrent updates.
        // This is guaranteed by the fact the old_parent_buf, old_buf are
        // retrieved from arakoon in the first place.

        const ObjectId parent_id(*(old_treeconfig.parent_volume));
        const std::string parent_key(make_key_(parent_id));
        ara::buffer old_parent_buf;
        ObjectRegistrationPtr old_parent_reg(find_throw_(parent_id,
                                                         parent_key,
                                                         old_parent_buf));

        const ObjectTreeConfig& parent_treeconfig = old_parent_reg->treeconfig;
        if (parent_treeconfig.object_type != ObjectType::Volume and
            parent_treeconfig.object_type != ObjectType::Template)
        {
            // If somebody messed up arakoon we still could proceed with just
            // deleting the clone in this case.
            // But requiring consistency seems safest now.
            LOG_ERROR("INCONSISTENCY: parent " << parent_id << " of " << id <<
                      " is neither a volume nor a template");
            throw InconsistencyException() << error_object_id(id)
                                           << error_parent_volume_id(parent_id)
                                           << error_desc("parent is neither a volume nor template");
        }

        ObjectTreeConfig::Descendants new_descendants(parent_treeconfig.descendants);

        {
            const auto nremoved = new_descendants.erase(id);
            if (nremoved == 0)
            {
                // This is not necessarily an inconsistency, as a concurrent
                // unregister could have deleted the same clone already.
                LOG_ERROR("Parent " << parent_id << " does not refer to " << id <<
                          " anymore. Concurrently deleted?");
                throw ConflictingUpdateException() << error_object_id(id)
                                                   << error_parent_volume_id(parent_id)
                                                   << error_desc("parent does not refer to clone anymore");
            }
        }

        ObjectRegistration new_parent_reg(old_parent_reg->getNS(),
                                          old_parent_reg->volume_id,
                                          old_parent_reg->node_id,
                                          ObjectTreeConfig::makeParent(parent_treeconfig.object_type,
                                                                       new_descendants,
                                                                       parent_treeconfig.parent_volume),
                                          old_parent_reg->owner_tag,
                                          old_parent_reg->foc_config_mode);

        //atomic update of template and deletion of clone
        seq.add_assert(parent_key,
                       old_parent_buf);
        seq.add_set(parent_key,
                    serialize_volume_registration(new_parent_reg));
    }

    seq.add_assert(key, old_buf);
    seq.add_delete(key);

    // unsupported for now
    //  - concurrent clone_from_template calls: could fail with
    //    ConflictingUpdateException instead of one of:
    //        - InvalidOperation (in case clone was first)
    //        - ObjectNotRegistered (in case unregister was first)
    //  - concurrent migrate calls
    //  - ...
}

void
ObjectRegistry::prepare_unregister_volumoid_(ara::sequence& seq,
                                             const ObjectId& id)
{
    LOG_DEBUG(id);

    with_owned_volume_(id,
                       node_id(),
                       [&](const std::string& key,
                           const ara::buffer& old_buf,
                           const ObjectRegistration& old_reg)
                       {
                           do_prepare_unregister_volumoid_(id,
                                                           key,
                                                           old_buf,
                                                           old_reg,
                                                           seq);
                       });
}

void
ObjectRegistry::prepare_unregister_file_(ara::sequence& seq,
                                         const ObjectId& id)
{
    LOG_DEBUG(id);

    with_owned_volume_(id,
                       node_id(),
                       [&](const std::string& key,
                           const ara::buffer& old_buf,
                           const ObjectRegistration& old_reg)
                       {
                           VERIFY(old_reg.treeconfig.object_type == ObjectType::File);
                           seq.add_assert(key, old_buf);
                           seq.add_delete(key);

                           //unsupported for now
                           //  - concurrent migrate calls
                           //  - ...
                       });
}

void
ObjectRegistry::unregister(const ObjectId& id)
{
    LOG_INFO(ID() << ": unregistering " << id);

    ObjectRegistrationPtr reg(find_throw(id));
    const ObjectType otype = reg->treeconfig.object_type;

    switch (otype)
    {
    case ObjectType::File:
        run_sequence_(id,
                      "unregister file",
                      [&](ara::sequence& seq)
                      {
                          prepare_unregister_file_(seq, id);
                      },
                      yt::RetryOnArakoonAssert::F);
        break;
    case ObjectType::Volume:
    case ObjectType::Template:
        run_sequence_(id,
                      (otype == ObjectType::Volume) ?
                      "unregister volume" :
                      "unregister template",
                      [&](ara::sequence& seq)
                      {
                          prepare_unregister_volumoid_(seq, id);
                      },
                      yt::RetryOnArakoonAssert::T);
        break;
    }
}

void
ObjectRegistry::wipe_out(const ObjectId& oid)
{
    LOG_INFO(ID() << ": wiping out " << oid);

    try
    {
        larakoon_->delete_prefix(make_key_(oid));
    }
    catch (ara::error_not_found&)
    {
        LOG_WARN(ID() << ": object " << oid <<
                 " is not present anymore - someone else beat is to removing it it?");
    }
}

ObjectRegistrationPtr
ObjectRegistry::do_prepare_set_volume_as_template_(const std::string& key,
                                                   const ara::buffer& old_buf,
                                                   const ObjectRegistration& old_reg,
                                                   ara::sequence& seq)
{
    ObjectRegistrationPtr reg;

    const ObjectTreeConfig& old_treeconfig = old_reg.treeconfig;

    switch (old_treeconfig.object_type)
    {
    case ObjectType::File:
        {
            LOG_ERROR(ID() << ": cannot convert a file into a template " <<
                      old_reg.volume_id);
            throw InvalidOperationException() <<
                error_object_id(old_reg.volume_id) <<
                error_desc("cannot convert a file into a template");
        }
    case ObjectType::Volume:
        {
            if (not old_treeconfig.descendants.empty())
            {
                LOG_ERROR(ID() << ": cannot convert volume to template as it still has descendants");
                throw InvalidOperationException() <<
                error_object_id(old_reg.volume_id) <<
                error_desc("cannot convert a volume with descendants into a template");
            }

            if (old_treeconfig.parent_volume)
            {
                ObjectRegistrationPtr parent(find_throw(*old_treeconfig.parent_volume));
                // do we even want to allow this or refuse templates from templates as well?
                if (parent->treeconfig.object_type != ObjectType::Template)
                {
                    LOG_ERROR(ID() << ": cannot create template with a non-template parent");
                    throw InvalidOperationException() <<
                        error_object_id(old_reg.volume_id) <<
                        error_desc("cannot templatize a clone whose parent is not a template");
                }
            }

            reg = boost::make_shared<ObjectRegistration>(old_reg.getNS(),
                                                         old_reg.volume_id,
                                                         node_id_,
                                                         ObjectTreeConfig::makeTemplate(old_treeconfig.parent_volume),
                                                         old_reg.owner_tag,
                                                         old_reg.foc_config_mode);
            seq.add_assert(key,
                           old_buf);
            seq.add_set(key,
                        serialize_volume_registration(*reg));
            break;
        }
    case ObjectType::Template:
        {
            LOG_WARN(ID() << ": volume " << old_reg.volume_id <<
                     " was already set as template");
            reg = boost::make_shared<ObjectRegistration>(old_reg);
            break;
        }
    }

    return reg;
}

ObjectRegistrationPtr
ObjectRegistry::prepare_set_volume_as_template_(ara::sequence& seq,
                                                const ObjectId& id)
{
    LOG_DEBUG(id);

    ObjectRegistrationPtr reg;

    with_owned_volume_(id,
                       node_id(),
                       [&](const std::string& key,
                           const ara::buffer& old_buf,
                           const ObjectRegistration& old_reg)
                       {
                           reg = do_prepare_set_volume_as_template_(key,
                                                                    old_buf,
                                                                    old_reg,
                                                                    seq);
                       });

    VERIFY(reg);
    return reg;
}

ObjectRegistrationPtr
ObjectRegistry::set_volume_as_template(const ObjectId& vol_id)
{
    LOG_INFO(ID() << ": setting " << vol_id << " as template");
    ObjectRegistrationPtr reg;

    run_sequence_(vol_id,
                  "set volume as template",
                  [&](ara::sequence& seq)
                  {
                      reg = prepare_set_volume_as_template_(seq, vol_id);
                  },
                  yt::RetryOnArakoonAssert::F);

    VERIFY(reg);
    return reg;
}

void
ObjectRegistry::TESTONLY_add_to_registry(const ObjectRegistration& reg)
{
    larakoon_->run_sequence("add to registry",
                            [&](ara::sequence& seq)
                            {
                                const std::string key(make_key_(reg.volume_id));
                                seq.add_assert(key, ara::None());
                                seq.add_set(key, serialize_volume_registration(reg));
                            },
                            yt::RetryOnArakoonAssert::F);
}

ObjectRegistrationPtr
ObjectRegistry::find_owned_throw(const ObjectId& id)
{
    auto reg = find_throw(id);
    auto owner = node_id();
    if (reg->node_id != owner)
    {
        LOG_ERROR(ID() << ": volume " << id << " is not hosted by " << owner <<
                  " but by " << reg->node_id);
        throw WrongOwnerException() << error_object_id(id)
                                    << error_node_id(owner)
                                    << error_other_node_id(reg->node_id);
    }
    return reg;
}

ObjectRegistrationPtr
ObjectRegistry::do_prepare_set_foc_config_mode_(const std::string& key,
                                                const ara::buffer& old_buf,
                                                const ObjectRegistration& old_reg,
                                                const FailOverCacheConfigMode foc_cm,
                                                ara::sequence& seq)
{
    auto reg(boost::make_shared<ObjectRegistration>(old_reg));
    reg->foc_config_mode = foc_cm;
    seq.add_assert(key,
                   old_buf);
    seq.add_set(key,
                serialize_volume_registration(*reg));

    return reg;
}

ObjectRegistrationPtr
ObjectRegistry::prepare_set_foc_config_mode_(ara::sequence& seq,
                                             const ObjectId& id,
                                             const FailOverCacheConfigMode foc_cm)
{
    LOG_DEBUG(id);

    ObjectRegistrationPtr reg;

    with_owned_volume_(id,
                       node_id(),
                       [&](const std::string& key,
                           const ara::buffer& old_buf,
                           const ObjectRegistration& old_reg)
                       {
                           reg = do_prepare_set_foc_config_mode_(key,
                                                                 old_buf,
                                                                 old_reg,
                                                                 foc_cm,
                                                                 seq);
                       });

    VERIFY(reg);
    return reg;
}

ObjectRegistrationPtr
ObjectRegistry::set_foc_config_mode(const ObjectId& vol_id,
                                    FailOverCacheConfigMode foc_cm)
{
    ObjectRegistrationPtr reg;

    run_sequence_(vol_id,
                  "set FOC config mode",
                  [&](ara::sequence& seq)
                  {
                      reg = prepare_set_foc_config_mode_(seq, vol_id, foc_cm);
                  },
                  yt::RetryOnArakoonAssert::F);

    VERIFY(reg);
    return reg;
}

}
