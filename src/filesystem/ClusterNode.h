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

#include <boost/thread/future.hpp>

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

    using WriteResult = std::pair<size_t, volumedriver::DtlInSync>;
    using WriteFuture = boost::future<WriteResult>;

    virtual WriteFuture
    write(const Object&,
          const uint8_t* buf,
          size_t size,
          off_t off) = 0;

    using ReadFuture = boost::future<size_t>;

    virtual ReadFuture
    read(const Object&,
         uint8_t* buf,
         size_t size,
         off_t off) = 0;

    using SyncFuture = boost::future<volumedriver::DtlInSync>;

    virtual SyncFuture
    sync(const Object&) = 0;

    virtual uint64_t
    get_size(const Object&) = 0;

    virtual volumedriver::ClusterMultiplier
    get_cluster_multiplier(const Object&) = 0;

    virtual volumedriver::CloneNamespaceMap
    get_clone_namespace_map(const Object&) = 0;

    virtual std::vector<volumedriver::ClusterLocation>
    get_page(const Object&,
             const volumedriver::ClusterAddress) = 0;

    virtual void
    resize(const Object&,
           uint64_t newsize) = 0;

    virtual void
    unlink(const Object&) = 0;

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
