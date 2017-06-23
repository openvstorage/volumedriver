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

#ifndef VD_DTL_CLIENT_H_
#define VD_DTL_CLIENT_H_

// PRAGMA_...
#include <youtils/Assert.h>

#include "ClientInterface.h"

PRAGMA_IGNORE_WARNING_BEGIN("-Wnon-virtual-dtor");
#include "failovercache/Protocol-capnp.h"
PRAGMA_IGNORE_WARNING_END;

#include "../OwnerTag.h"
#include "../SCO.h"
#include "../SCOProcessorInterface.h"

#include <boost/asio.hpp>
#include <boost/chrono.hpp>
#include <boost/optional.hpp>
#include <boost/thread/future.hpp>
#include <boost/variant/variant.hpp>

#include <youtils/AsioServiceManager.h>
#include <youtils/IOException.h>
#include <youtils/Logging.h>

namespace capnp
{
class MallocMessageBuilder;
}

namespace volumedriver
{

class ClusterLocation;
class FailOverCacheConfig;
class FailOverCacheEntry;

namespace failovercache
{

// push these to ClientInterface?
MAKE_EXCEPTION(ServerException, fungi::IOException);
MAKE_EXCEPTION(CapnProtoClientException, fungi::IOException);
MAKE_EXCEPTION(ProtocolException, CapnProtoClientException);
MAKE_EXCEPTION(RequestCancelledException, CapnProtoClientException);
MAKE_EXCEPTION(RequestTimeoutException, CapnProtoClientException);
MAKE_EXCEPTION(ConnectTimeoutException, CapnProtoClientException);

class CapnProtoClient
    : public ClientInterface
{
private:
    friend class ClientInterface;

    CapnProtoClient(const FailOverCacheConfig&,
                    const youtils::ImplicitStrand,
                    const OwnerTag,
                    boost::asio::io_service&,
                    const backend::Namespace&,
                    const LBASize,
                    const ClusterMultiplier,
                    const ClientInterface::MaybeMilliSeconds& request_timeout,
                    const ClientInterface::MaybeMilliSeconds& connect_timeout,
                    const size_t burst_size = 256);
public:
    ~CapnProtoClient();

    CapnProtoClient(const CapnProtoClient&) = delete;

    CapnProtoClient&
    operator=(const CapnProtoClient&) = delete;

    boost::future<void>
    addEntries(std::vector<FailOverCacheEntry>) override final;

    boost::future<void>
    addEntries(const std::vector<ClusterLocation>&,
               uint64_t addr,
               const uint8_t*) override final;

    // returns the SCO size - 0 indicates a problem.
    // Z42: throw instead!
    uint64_t
    getSCOFromFailOver(SCO sco,
                       SCOProcessorFun fun) override final;

    void
    getSCORange(SCO& oldest,
                SCO& youngest) override final;

    void
    clear() override final;

    boost::future<void>
    flush() override final;

    void
    removeUpTo(const SCO) noexcept override final;

    void
    getEntries(SCOProcessorFun fun) override final;

    std::pair<ClusterLocation, ClusterLocation>
    getRange();

    class AsioClient;

    using RawBufDesc = std::pair<const uint8_t*, size_t>;
    using RawBufOrEntryVec = boost::variant<RawBufDesc,
                                            std::vector<FailOverCacheEntry>>;

private:
    DECLARE_LOGGER("DtlCapnProtoClient");

    std::shared_ptr<AsioClient> asio_client_;
    OwnerTag owner_tag_;
    size_t burst_size_;

    void
    open_();

    void
    close_();

    template<typename Fun>
    size_t
    get_entries_(ClusterLocation loc,
                 size_t count,
                 Fun&&);

    template<typename Rsp,
             typename Fun>
    boost::future<void>
    submit_(capnp::MessageBuilder&,
            RawBufOrEntryVec,
            typename Rsp::Reader (protocol::ResponseData::Reader::*get_reader)() const,
            Fun&& fun);
};

}

}

#endif // !VD_DTL_CLIENT_H_
