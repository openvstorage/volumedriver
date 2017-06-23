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

#include "Backend.h"
#include "CapnProtoDispatcher.h"
#include "FailOverCacheProtocol.h"

#include "../FailOverCacheEntry.h"

#include <capnp/serialize.h>

#include <kj/array.h>

#include <youtils/Assert.h>
#include <youtils/Catchers.h>

namespace volumedriver
{

namespace failovercache
{

namespace yt = youtils;
namespace proto = protocol;

std::vector<uint8_t>
CapnProtoDispatcher::operator()(capnp::MessageBuilder& mb,
                                kj::Array<capnp::word> capnp_buf,
                                std::unique_ptr<uint8_t[]> data_buf,
                                size_t bufsize)
{
    ASSERT(data_buf or bufsize == 0);
    ASSERT((bufsize % sizeof(capnp::word)) == 0);

    capnp::FlatArrayMessageReader mr(capnp_buf.asPtr());

    std::vector<uint8_t> res;

    try
    {
        auto rroot(mb.initRoot<proto::Response>());
        proto::Response::Rsp::Builder rb(rroot.initRsp());
        proto::ResponseData::Builder rspb(rb.initOk());

        const proto::Request::Reader broot(mr.getRoot<proto::Request>());
        const proto::RequestData::Reader rr(broot.getRequestData());

        switch (rr.which())
        {
#define CASE(x)  case proto::RequestData::Which::x

            CASE(OPEN_REQUEST):
                open_(rr.getOpenRequest(),
                      rspb.initOpenResponse());
            break;
            CASE(CLOSE_REQUEST):
                close_(rr.getCloseRequest(),
                       rspb.initCloseResponse());
            break;
            CASE(GET_RANGE_REQUEST):
                get_range_(rr.getGetRangeRequest(),
                           rspb.initGetRangeResponse());
            break;
            CASE(ADD_ENTRIES_REQUEST):
                add_entries_(rr.getAddEntriesRequest(),
                             rspb.initAddEntriesResponse(),
                             std::move(data_buf),
                             bufsize);
            break;
            CASE(GET_ENTRIES_REQUEST):
                res = get_entries_(rr.getGetEntriesRequest(),
                                   rspb.initGetEntriesResponse());
            break;
            CASE(FLUSH_REQUEST):
                flush_(rr.getFlushRequest(),
                       rspb.initFlushResponse());
            break;
            CASE(CLEAR_REQUEST):
                clear_(rr.getClearRequest(),
                       rspb.initClearResponse());
            break;
            CASE(REMOVE_UP_TO_REQUEST):
                remove_up_to_(rr.getRemoveUpToRequest(),
                              rspb.initRemoveUpToResponse());
            break;
        default:
            {
                std::stringstream ss;
                ss << "unknown request " << static_cast<uint16_t>(rr.which());
                LOG_ERROR(ss.str());
                throw fungi::IOException(ss.str());
            }
#undef CASE
        }
    }
    // TODO: better exception translation
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("Error processing request: " << EWHAT);

            const std::string str(EWHAT);
            capnp::Text::Reader tr(str.c_str(),
                                   str.size());

            auto rroot(mb.initRoot<proto::Response>());
            proto::Response::Rsp::Builder rb(rroot.initRsp());
            proto::Error::Builder eb(rb.initError());
            eb.setMessage(tr);
            eb.setCode(0);
            res.resize(0);
        });

    return res;
}

void
CapnProtoDispatcher::open_(proto::OpenRequest::Reader&& reader,
                           proto::OpenResponse::Builder&&)
{
    LOG_INFO(this << ": registering " << reader.getNspace().cStr());
    proto_.do_register_(reader.getNspace().cStr(),
                        ClusterSize(reader.getClusterSize()),
                        OwnerTag(reader.getOwnerTag()));
}

void
CapnProtoDispatcher::close_(proto::CloseRequest::Reader&&,
                            proto::CloseResponse::Builder&&)
{
    LOG_INFO(this << ": closing");
    proto_.do_unregister_();
}

void
CapnProtoDispatcher::flush_(proto::FlushRequest::Reader&&,
                            proto::FlushResponse::Builder&&)
{
    proto_.do_flush_();
}

void
CapnProtoDispatcher::clear_(proto::ClearRequest::Reader&&,
                            proto::ClearResponse::Builder&&)
{
    LOG_INFO(this << ": wiping all entries");
    proto_.do_clear_();
}

void
CapnProtoDispatcher::get_range_(proto::GetRangeRequest::Reader&&,
                                proto::GetRangeResponse::Builder&& builder)
{
    ClusterLocation oldest;
    ClusterLocation youngest;

    std::tie(oldest, youngest) = proto_.do_get_range_();

    builder.setBegin(reinterpret_cast<const uint64_t&>(oldest));
    builder.setEnd(reinterpret_cast<const uint64_t&>(youngest));
}

std::vector<uint8_t>
CapnProtoDispatcher::get_entries_(proto::GetEntriesRequest::Reader&& reader,
                                  proto::GetEntriesResponse::Builder&& builder)
{
    VERIFY(proto_.cache_);
    const size_t csize = proto_.cache_->cluster_size();

    const uint64_t b = reader.getStartClusterLocation();
    ClusterLocation loc(reinterpret_cast<const ClusterLocation&>(b));
    const size_t max = reader.getNumEntries();
    std::vector<uint8_t> buf(max * csize);

    if (loc == ClusterLocation(0))
    {
        std::tie(loc, std::ignore) = proto_.do_get_range_();
    }

    // TODO: this incurs an extra copy into Entry::data
    struct Entry
    {
        ClusterLocation loc;
        uint64_t addr;

        Entry(ClusterLocation l,
              uint64_t a)
            : loc(l)
            , addr(a)
        {}
    };

    // TODO: this causes an extra copy - figure out if capnp::List<proto::ClusterEntry>
    // can be dynamically resized (opposed to init'ing it with a fixed size)
    std::vector<Entry> entries;
    entries.reserve(max);
    size_t off = 0;

    auto fun([csize,
              &buf,
              &entries,
              &off](ClusterLocation loc,
                        int64_t addr,
                        const uint8_t* data,
                        int64_t size)
             {
                 entries.emplace_back(loc,
                                      addr);
                 ASSERT(size == static_cast<int64_t>(csize));
                 memcpy(buf.data() + off,
                        data,
                        size);
                 off += size;
             });

    proto_.cache_->get_entries(loc,
                               max,
                               std::move(fun));

    VERIFY(entries.size() * csize <= buf.size());
    buf.resize(entries.size() * csize);

    capnp::List<proto::ClusterEntry>::Builder
        lbuilder(builder.initEntries(entries.size()));
    for (size_t i = 0; i < entries.size(); ++i)
    {
        auto e = lbuilder[i];
        e.setAddress(entries[i].addr);
        e.setClusterLocation(reinterpret_cast<const uint64_t&>(entries[i].loc));
    }

    return buf;
}

void
CapnProtoDispatcher::remove_up_to_(proto::RemoveUpToRequest::Reader&& reader,
                                   proto::RemoveUpToResponse::Builder&&)
{
    const uint64_t l = reader.getClusterLocation();
    const ClusterLocation loc(reinterpret_cast<const ClusterLocation&>(l));
    VERIFY(loc.offset() == std::numeric_limits<SCOOffset>::max());

    LOG_INFO(this << ": removing up to " << loc.sco());
    proto_.do_remove_up_to_(loc.sco());
}

void
CapnProtoDispatcher::add_entries_(proto::AddEntriesRequest::Reader&& reader,
                                  proto::AddEntriesResponse::Builder&&,
                                  std::unique_ptr<uint8_t[]> rbuf,
                                  size_t bufsize)
{
    VERIFY(proto_.cache_);

    const capnp::List<proto::ClusterEntry>::Reader lr(reader.getEntries());
    std::vector<FailOverCacheEntry> entries;
    entries.reserve(lr.size());

    VERIFY(rbuf or not lr.size());

    size_t off = 0;
    const size_t csize = proto_.cache_->cluster_size();

    for (const auto& er : lr)
    {
        VERIFY(off + csize <= bufsize);

        ClusterLocation loc;
        reinterpret_cast<uint64_t&>(loc) = er.getClusterLocation();
        entries.emplace_back(loc,
                             er.getAddress(),
                             rbuf.get() + off,
                             csize);

        off += csize;
    }

    proto_.do_add_entries_(std::move(entries),
                           std::move(rbuf));
}

}

}
