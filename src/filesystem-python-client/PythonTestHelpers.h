// Copyright 2015 Open vStorage NV
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
