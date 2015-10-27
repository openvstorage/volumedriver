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

#ifndef VFS_CLUSTER_NODE_H_
#define VFS_CLUSTER_NODE_H_

#include "ClusterNodeConfig.h"
#include "Object.h"
#include "NodeId.h"

#include <youtils/Logging.h>

#include <volumedriver/Types.h>

namespace volumedriverfs
{

class ObjectRouter;

class ClusterNode
{
public:
    virtual ~ClusterNode()
    {}

    // XXX: we have to pass the size by a ptr instead of a reference since
    // our use of variadic templates in ObjectRouter leads to
    // "inconsistent parameter pack deduction with ‘long unsigned int&’ and
    // ‘long unsigned int’" error messages when using a reference.
    virtual void
    write(const Object& obj,
          const uint8_t* buf,
          size_t* size,
          off_t off) = 0;

    virtual void
    read(const Object& obj,
         uint8_t* buf,
         size_t* size,
         off_t off) = 0;

    virtual void
    sync(const Object& obj) = 0;

    virtual uint64_t
    get_size(const Object& obj) = 0;

    virtual void
    resize(const Object& obj,
           uint64_t newsize) = 0;

    virtual void
    unlink(const Object& obj) = 0;

    const ClusterNodeConfig config;

protected:
    ClusterNode(ObjectRouter& vrouter_,
                const ClusterNodeConfig& cfg);

    ObjectRouter& vrouter_;

private:
    DECLARE_LOGGER("VFSClusterNode");
};

}

#endif // !VFS_CLUSTER_NODE_H_
