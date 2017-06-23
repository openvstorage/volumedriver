// Copyright (C) 2017 iNuron NV
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

#ifndef VD_DTL_CAPNPROTO_DISPATCHER_H_
#define VD_DTL_CAPNPROTO_DISPATCHER_H_

// PRAGMA_...
#include <youtils/Assert.h>

PRAGMA_IGNORE_WARNING_BEGIN("-Wnon-virtual-dtor");
#include "failovercache/Protocol-capnp.h"
PRAGMA_IGNORE_WARNING_END;

#include <youtils/Logging.h>

namespace capnp
{
class MessageBuilder;
}

namespace volumedriver
{

namespace failovercache
{

class FailOverCacheProtocol;

class CapnProtoDispatcher
{
public:
    explicit CapnProtoDispatcher(FailOverCacheProtocol& p)
        : proto_(p)
    {}

    ~CapnProtoDispatcher() = default;

    std::vector<uint8_t>
    operator()(capnp::MessageBuilder&,
               kj::Array<capnp::word>,
               std::unique_ptr<uint8_t[]>,
               size_t bufsize);

private:
    DECLARE_LOGGER("DtlCapnProtoDispatcher");

    FailOverCacheProtocol& proto_;

    void
    open_(protocol::OpenRequest::Reader&&,
          protocol::OpenResponse::Builder&&);

    void
    close_(protocol::CloseRequest::Reader&&,
           protocol::CloseResponse::Builder&&);

    void
    flush_(protocol::FlushRequest::Reader&&,
           protocol::FlushResponse::Builder&&);

    void
    clear_(protocol::ClearRequest::Reader&&,
           protocol::ClearResponse::Builder&&);

    void
    get_range_(protocol::GetRangeRequest::Reader&&,
               protocol::GetRangeResponse::Builder&&);

    std::vector<uint8_t>
    get_entries_(protocol::GetEntriesRequest::Reader&&,
                 protocol::GetEntriesResponse::Builder&&);

    void
    remove_up_to_(protocol::RemoveUpToRequest::Reader&&,
                  protocol::RemoveUpToResponse::Builder&&);

    void
    add_entries_(protocol::AddEntriesRequest::Reader&&,
                 protocol::AddEntriesResponse::Builder&&,
                 std::unique_ptr<uint8_t[]>,
                 size_t bufsize);
};

}

}

#endif //! VD_DTL_CAPNPROTO_DISPATCHER_H_
