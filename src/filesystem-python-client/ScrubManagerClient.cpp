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

#include "IterableConverter.h"
#include "ScrubManagerClient.h"

#include <boost/lexical_cast.hpp>
#include <boost/python.hpp>
#include <boost/python/class.hpp>
#include <boost/python/return_value_policy.hpp>

#include <youtils/LockedArakoon.h>

#include <volumedriver/ScrubReply.h>

#include <filesystem/ClusterId.h>
#include <filesystem/NodeId.h>
#include <filesystem/ObjectRegistry.h>
#include <filesystem/ScrubManager.h>

namespace volumedriverfs
{

namespace python
{

namespace ara = arakoon;
namespace bpy = boost::python;
namespace vd = volumedriver;
namespace yt = youtils;

using namespace std::literals::string_literals;

namespace
{

class Wrapper
{
public:
    Wrapper(const ClusterId& cluster_id,
                       const NodeId& node_id,
                       const ara::ClusterID& ara_cluster_id,
                       const std::vector<ara::ArakoonNodeConfig>& ara_node_configs)
        : registry_(cluster_id,
                    node_id,
                    std::make_shared<yt::LockedArakoon>(ara_cluster_id,
                                                        ara_node_configs))
        , scrub_manager_(registry_,
                         registry_.locked_arakoon())
    {}

    ~Wrapper() = default;

    std::vector<scrubbing::ScrubReply>
    get_parent_scrubs()
    {
        ScrubManager::ParentScrubs scrubs(scrub_manager_.get_parent_scrubs());
        std::vector<scrubbing::ScrubReply> vec;
        vec.reserve(scrubs.size());

        for (auto&& s : scrubs)
        {
            vec.emplace_back(std::move(s.first));
        }

        return vec;
    }

    std::vector<scrubbing::ScrubReply>
    get_clone_scrubs()
    {
        return scrub_manager_.get_clone_scrubs();
    }

    ScrubManager::ClonePtrList
    get_scrub_tree(const scrubbing::ScrubReply& reply)
    {
        return scrub_manager_.get_scrub_tree(reply);
    }

private:
    ObjectRegistry registry_;
    ScrubManager scrub_manager_;
};

std::string
scrub_reply_repr(const scrubbing::ScrubReply& reply)
{
    return boost::lexical_cast<std::string>(reply);
}

std::string
scrub_reply_namespace(const scrubbing::ScrubReply& reply)
{
    return reply.ns_.str();
}

std::string
scrub_reply_snapshot_name(const scrubbing::ScrubReply& reply)
{
    return reply.snapshot_name_.str();
}

std::string
scrub_reply_scrub_result_name(const scrubbing::ScrubReply& reply)
{
    return reply.scrub_result_name_;
}

std::string
clone_repr(const ScrubManager::Clone* clone)
{
    return "Clone("s + clone->id.str() +")"s;
}

std::string
clone_id(const ScrubManager::Clone* clone)
{
    return clone->id.str();
}

ScrubManager::ClonePtrList
clone_children(const ScrubManager::Clone* clone)
{
    return clone->clones;
}

}

void
ScrubManagerClient::registerize()
{
    bpy::class_<scrubbing::ScrubReply>("ScrubReply",
                                       "debugging helper for the Scrubber + ScrubManager",
                                       bpy::init<const std::string&>
                                       ((bpy::args("scrub_reply_str")),
                                        "Create a ScrubReply from its serialized representation\n"
                                        "@param scrub_reply_str, string, serialized representation of ScrubReply as returned by the Scrubber\n"))
        .def("__repr__",
             scrub_reply_repr)
        .def("__eq__",
             &scrubbing::ScrubReply::operator==)
        .def("namespace",
             scrub_reply_namespace,
             "@return string, Namespace\n")
        .def("snapshot_name",
             scrub_reply_snapshot_name,
             "@return string, SnapshotName\n")
        .def("scrub_result_name",
             scrub_reply_scrub_result_name,
             "@return string, backend object name\n")
        ;

    REGISTER_ITERABLE_CONVERTER(std::vector<scrubbing::ScrubReply>);
    REGISTER_ITERABLE_CONVERTER(ScrubManager::ClonePtrList);

    bpy::class_<ScrubManager::Clone,
                boost::noncopyable,
                boost::shared_ptr<ScrubManager::Clone>>("Clone",
                                                        "An entry in a scrub family tree",
                                                        bpy::no_init)
        .def("__repr__",
             clone_repr)
        .def("id",
             clone_id)
        .def("children",
             clone_children)
        ;

    bpy::class_<Wrapper,
                boost::noncopyable,
                boost::shared_ptr<Wrapper>>("ScrubManagerClient",
                                            "debugging tool for the ScrubManager",
                                            bpy::init<const ClusterId&,
                                            const NodeId&,
                                            const ara::ClusterID&,
                                            const std::vector<ara::ArakoonNodeConfig>&>
                                            ((bpy::args("vrouter_cluster_id"),
                                              bpy::args("vrouter_node_id"),
                                              bpy::args("arakoon_cluster_id"),
                                              bpy::args("arakoon_node_configs")),
                                             "Create a ScrubWrapperClient\n"
                                             "@param vrouter_cluster_id, string, ClusterId\n"
                                             "@param vrouter_node_id, string, NodeId\n"
                                             "@param arakoon_cluster_id, string Arakoon ClusterId\n"
                                             "@param arakoon_node_configs, [ ArakoonNodeConfig ]\n"))
        .def("get_parent_scrubs",
             &Wrapper::get_parent_scrubs,
             "Return a list of ScrubReplies on the parent queue\n")
        .def("get_clone_scrubs",
             &Wrapper::get_clone_scrubs,
             "Return a list of ScrubReplies on the clone queue\n")
        .def("get_scrub_tree",
             &Wrapper::get_scrub_tree,
             (bpy::args("scrub_reply")),
             "Return the tree of clones a ScrubReply is queued for\n"
             "@param scrub_reply, ScrubReply\n"
             "@return Clone tree\n")
        ;
}

}

}
