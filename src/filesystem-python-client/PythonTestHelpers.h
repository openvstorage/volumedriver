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

#ifndef VFS_PYTHON_TEST_HELPERS_H_
#define VFS_PYTHON_TEST_HELPERS_H_

#include <map>

#include <youtils/ArakoonNodeConfig.h>
#include <youtils/UpdateReport.h>

#include <filesystem/ClusterId.h>
#include <filesystem/ClusterNodeStatus.h>
#include <filesystem/NodeId.h>

namespace volumedriverfs
{

namespace python
{

// Methods used by a python tester to check exception translation over boost-python
// TODO: these could all go into an anon namespace
struct PythonTestHelpers
{
    static std::string
    generate_exceptions(uint32_t type);

    //to test correct mapping to dict(string, State)
    static std::map<NodeId, ClusterNodeStatus::State>
    generate_node_status_map();

    // to test correct mapping to list(dict(string, string))
    static youtils::UpdateReport
    generate_update_report();

    // not that interesting in itself, but helpful for testing python conversions
    template<typename T>
    static T
    reflect(const T& t)
    {
        return t;
    }

    static ClusterId
    stringy_things(const arakoon::ClusterID& cluster_id,
                   const arakoon::NodeID& node_id)
    {
        return ClusterId(cluster_id.str() + std::string(":") + node_id.str());
    }

    static MessagePort
    arithmeticy_things(const XmlRpcPort xport, const FailoverCachePort fport)
    {
        return MessagePort(static_cast<uint16_t>(xport) +
                           static_cast<uint16_t>(fport));
    }

    static void
    registerize();
};

}

}

#endif // !VFS_PYTHON_TEST_HELPERS_H_
