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

#include "ObjectRegistryClient.h"

#include "IntegerConverter.h"
#include "IterableConverter.h"
#include "StringyConverter.h"

#include <list>

#include <boost/make_shared.hpp>
#include <boost/python/class.hpp>
#include <boost/python/copy_const_reference.hpp>
#include <boost/python/enum.hpp>
#include <boost/python/make_constructor.hpp>
#include <boost/python/register_ptr_to_python.hpp>
#include <boost/python/return_value_policy.hpp>

#include <youtils/LockedArakoon.h>
#include <youtils/Logger.h>

#include <volumedriver/OwnerTag.h>

#include <filesystem/ObjectRegistration.h>
#include <filesystem/ObjectRegistry.h>

namespace volumedriverfs
{

namespace python
{

namespace ara = arakoon;
namespace be = backend;
namespace bpy = boost::python;
namespace vd = volumedriver;
namespace yt = youtils;

namespace
{

class Wrapper
{
    const ClusterId cluster_id_;
    const ara::ClusterID ara_cluster_id_;
    const std::vector<ara::ArakoonNodeConfig> ara_node_configs_;

    using Ptr = boost::shared_ptr<ObjectRegistry>;

    static const NodeId&
    default_node_id_()
    {
        static const NodeId nid("ObjectRegistryPythonClient");
        return nid;
    }

    Ptr
    registry_(const NodeId& node_id = default_node_id_()) const
    {
        auto larakoon(std::make_shared<yt::LockedArakoon>(ara_cluster_id_,
                                                          ara_node_configs_));
        return boost::make_shared<ObjectRegistry>(cluster_id_,
                                                  node_id,
                                                  larakoon);
    }

public:
    Wrapper(const ClusterId& cluster_id,
            const ara::ClusterID& ara_cluster_id,
            const std::vector<ara::ArakoonNodeConfig>& ara_node_configs)
        : cluster_id_(cluster_id)
        , ara_cluster_id_(ara_cluster_id)
        , ara_node_configs_(ara_node_configs)
    {}

    ~Wrapper() = default;

    Wrapper(const Wrapper&) = default;

    Wrapper&
    operator=(const Wrapper&) = default;

    std::list<ObjectId>
    list() const
    {
        return registry_()->list();
    }

    ObjectRegistrationPtr
    find(const ObjectId& oid) const
    {
        return registry_()->find(oid);
    }

    ObjectRegistrationPtr
    register_base_volume(const NodeId& owner,
                         const ObjectId& oid,
                         const std::string& nspace)
    {
        return registry_(owner)->register_base_volume(oid,
                                                      be::Namespace(nspace));
    }

    ObjectRegistrationPtr
    register_file(const NodeId& owner,
                  const ObjectId& oid)
    {
        return registry_(owner)->register_file(oid);
    }

    ObjectRegistrationPtr
    register_clone(const NodeId& owner,
                   const ObjectId& clone_id,
                   const std::string& clone_nspace,
                   const ObjectId& parent_id,
                   const boost::optional<std::string>& maybe_parent_snap)
    {
        boost::optional<vd::SnapshotName> ms;
        if (maybe_parent_snap)
        {
            ms = vd::SnapshotName(*maybe_parent_snap);
        }

        return registry_(owner)->register_clone(clone_id,
                                                be::Namespace(clone_nspace),
                                                parent_id,
                                                ms);
    }

    ObjectRegistrationPtr
    set_volume_as_template(const NodeId& owner,
                           const ObjectId& oid)
    {
        return registry_(owner)->set_volume_as_template(oid);
    }


    void
    unregister(ObjectRegistrationPtr& reg) const
    {
        registry_(reg->node_id)->unregister(reg->volume_id);
    }

    ObjectRegistrationPtr
    migrate(const ObjectId& oid,
            const NodeId& from,
            const NodeId& to)
    {
        return registry_()->migrate(oid,
                                    from,
                                    to);
    }
};

NodeId
registration_node_id(const ObjectRegistration* reg)
{
    return reg->node_id;
}

ObjectId
registration_object_id(const ObjectRegistration* reg)
{
    return reg->volume_id;
}

vd::OwnerTag
registration_owner_tag(const ObjectRegistration* reg)
{
    return reg->owner_tag;
}

FailOverCacheConfigMode
foc_config_mode(const ObjectRegistration* reg)
{
    return reg->foc_config_mode;
}

}

void
ObjectRegistryClient::registerize()
{
    REGISTER_ITERABLE_CONVERTER(std::list<ObjectId>);

    bpy::class_<ObjectRegistration,
                boost::shared_ptr<ObjectRegistration>>("ObjectRegistration",
                                                       "Object registration",
                                                       bpy::no_init)
        .def("node_id",
             registration_node_id,
             "@return string, NodeId\n")
        .def("object_id",
             registration_object_id,
             "@return string, ObjectId\n")
        .def("owner_tag",
             registration_owner_tag,
             "@return OwnerTag\n")
        .def("foc_config_mode",
             foc_config_mode,
             "@return DTLConfigMode\n")
        ;

    bpy::class_<Wrapper>("ObjectRegistryClient",
                         "debugging tool for the ObjectRegistry",
                         bpy::init<const ClusterId&,
                                   const ara::ClusterID&,
                                   const std::vector<ara::ArakoonNodeConfig>&>
                         ((bpy::args("vrouter_cluster_id"),
                           bpy::args("arakoon_cluster_id"),
                           bpy::args("arakoon_node_configs")),
                          "Create an ObjectRegistryClient\n"
                          "@param vrouter_cluster_id, string, ClusterId\n"
                          "@param arakoon_cluster_id, string, Arakoon ClusterId\n"
                          "@param arakoon_node_configs, [ ArakoonNodeConfig ]\n"))
        .def("list",
             &Wrapper::list,
             "get a list of registered ObjectId's")
        .def("find",
             &Wrapper::find,
             bpy::args("object_id"),
             "Look up an object's registration\n"
             "@param object_id, string, ObjectId\n"
             "@return ObjectRegistration")
        .def("register_base_volume",
             &Wrapper::register_base_volume,
             (bpy::args("node_id"),
              bpy::args("object_id"),
              bpy::args("nspace")),
             "Register a base volume\n"
             "@param node_id, string, NodeId of the owning node\n"
             "@param object_id, string, ObjectId of the volume\n"
             "@param nspace, string, backend namespace associated with the volume\n"
             "@return ObjectRegistration")
        .def("register_file",
             &Wrapper::register_file,
             (bpy::args("node_id"),
              bpy::args("object_id")),
             "Register a file\n"
             "@param node_id, string, NodeId of the owning node\n"
             "@param object_id, string, ObjectId of the file\n"
             "@return ObjectRegistration")
        .def("register_clone",
             &Wrapper::register_clone,
             (bpy::args("node_id"),
              bpy::args("clone_id"),
              bpy::args("clone_nspace"),
              bpy::args("parent_id"),
              bpy::args("maybe_parent_snap")),
             "Register a file\n"
             "@param node_id, string, NodeId of the owning node\n"
             "@param clone_id, string, ObjectId of the clone\n"
             "@param clone_nspace, string, backend namespace of the clone\n"
             "@param parent_id, string, ObjectId of the parent\n"
             "@param maybe_parent_snap, string or None, snapshot name of the parent\n"
             "@return ObjectRegistration")
        .def("unregister",
             &Wrapper::unregister,
             bpy::args("object_registration"),
             "Remove a registration\n"
             "@param object_registration, ObjectRegistration, registration to remove\n")
        .def("migrate",
             &Wrapper::migrate,
             (bpy::args("object_id"),
              bpy::args("from_node"),
              bpy::args("to_node")),
             "Change ownership of an object\n"
             "@param object_id, string, ObjectId of the object\n"
             "@param from_node, string, NodeId of the current owner\n"
             "@param to_node, string, NodeId of the new owner\n"
             "@param return ObjectRegistration\n")
        .def("set_volume_as_template",
             &Wrapper::set_volume_as_template,
             (bpy::args("node_id"),
              bpy::args("object_id")),
             "Mark a volume as template\n"
             "@param node_id, string, NodeId of the owner\n"
             "@param object_id, string, ObjectId of the volume\n"
             "@return ObjectRegistration\n")
        ;
}

}

}
