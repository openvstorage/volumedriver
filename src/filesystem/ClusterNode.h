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

#ifndef VFS_CLUSTER_NODE_H_
#define VFS_CLUSTER_NODE_H_

#include "NodeId.h"

#include <youtils/Logging.h>
#include <youtils/Uri.h>

#include <volumedriver/ClusterLocation.h>
#include <volumedriver/DtlInSync.h>
#include <volumedriver/Types.h>

namespace volumedriverfs
{

class Object;
class ObjectRouter;

class ClusterNode
{
public:
    virtual ~ClusterNode() = default;

    // XXX: we have to pass the size by a ptr instead of a reference since
    // our use of variadic templates in ObjectRouter leads to
    // "inconsistent parameter pack deduction with ‘long unsigned int&’ and
    // ‘long unsigned int’" error messages when using a reference.
    virtual void
    write(const Object& obj,
          const uint8_t* buf,
          size_t* size,
          off_t off,
          volumedriver::DtlInSync&) = 0;

    virtual void
    read(const Object& obj,
         uint8_t* buf,
         size_t* size,
         off_t off) = 0;

    virtual void
    sync(const Object&,
         volumedriver::DtlInSync&) = 0;

    virtual uint64_t
    get_size(const Object& obj) = 0;

    virtual volumedriver::ClusterMultiplier
    get_cluster_multiplier(const Object&) = 0;

    virtual volumedriver::CloneNamespaceMap
    get_clone_namespace_map(const Object&) = 0;

    virtual std::vector<volumedriver::ClusterLocation>
    get_page(const Object&,
             const volumedriver::ClusterAddress) = 0;

    virtual void
    resize(const Object& obj,
           uint64_t newsize) = 0;

    virtual void
    unlink(const Object& obj) = 0;

    const NodeId&
    node_id() const
    {
        return node_id_;
    }

    const youtils::Uri&
    uri() const
    {
        return uri_;
    }

protected:
    ClusterNode(ObjectRouter&_,
                const NodeId&,
                const youtils::Uri&);

    ObjectRouter& vrouter_;

private:
    DECLARE_LOGGER("VFSClusterNode");

    const NodeId node_id_;
    const youtils::Uri uri_;
};

}

#endif // !VFS_CLUSTER_NODE_H_
