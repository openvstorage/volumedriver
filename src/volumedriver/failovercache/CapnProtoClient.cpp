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

#include "CapnProtoClient.h"

#include "../FailOverCacheConfig.h"
#include "../FailOverCacheEntry.h"
#include "../FailOverCacheCommand.h"

#include <atomic>
#include <list>
#include <unordered_map>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>

#include <capnp/message.h>
#include <capnp/serialize.h>

#include <kj/array.h>

#include <youtils/AsioServiceManager.h>
#include <youtils/Assert.h>
#include <youtils/Catchers.h>
#include <youtils/InlineExecutor.h>
#include <youtils/ScopeExit.h>
#include <youtils/SourceOfUncertainty.h>

namespace volumedriver
{

namespace failovercache
{

namespace ba = boost::asio;
namespace be = backend;
namespace bs = boost::system;
namespace proto = volumedriver::failovercache::protocol;
namespace yt = youtils;

namespace
{

using Clock = std::chrono::steady_clock;

class RequestInfo;
using RequestInfoPtr = std::shared_ptr<RequestInfo>;

using BulkData = std::vector<uint8_t>;
using RequestResult = std::pair<kj::Array<capnp::word>, BulkData>;
using RequestResultFuture = boost::future<RequestResult>;
}

// AsioClient takes care of sending requests / receiving responses
// which are built by Client (using CapnProto) using boost::asio.
// Ownership / lifetime is hence managed by shared_ptrs. In a nutshell:
// - Client: shared_ptr<AsioClient>
//   - (asio network handler closures): shared_ptr<AsioClient>
//   - (asio timeout handler closures): shared_ptr<RequestInfo>
// - RequestInfo: shared_ptr<AsioClient>
// - AsioClient: shared_ptr<RequestInfo> (in map)
//
// The reference cycle between AsioClient and RequestInfo is
// broken up by AsioClient::shutdown which will abort all requests.

// TODO: sort out the timeout vs. cancellation
class CapnProtoClient::AsioClient
    : public std::enable_shared_from_this<CapnProtoClient::AsioClient>
{
public:
    AsioClient(ba::io_service&,
               const yt::ImplicitStrand,
               const backend::Namespace&);

    ~AsioClient();

    void
    connect(const std::string& host,
            uint16_t port,
            const ClientInterface::MaybeMilliSeconds&);

    template<typename Fun>
    void
    post(Fun&& fun)
    {
        if (strand_)
        {
            strand_->post(std::move(fun));
        }
        else
        {
            io_service_.post(std::move(fun));
        }
    }

    template<typename Ex>
    void
    shutdown(const Ex& ex) noexcept
    {
        try
        {
            post([ex,
                  self = shared_from_this(),
                  this]()
                 {
                     shutdown_(ex);
                 });
        }
        CATCH_STD_ALL_LOG_IGNORE(nspace_ << ": failed to schedule shutdown");
    }

    RequestResultFuture
    send_request(kj::Array<capnp::word> capnp_msg,
                 RawBufOrEntryVec,
                 const ClientInterface::MaybeMilliSeconds&);

    ba::io_service&
    io_service()
    {
        return io_service_;
    }

    template<typename Fun>
    void
    stranded_async_wait(ba::steady_timer& deadline,
                        Fun&& fun)
    {
        if (strand_)
        {
            deadline.async_wait(strand_->wrap(std::move(fun)));
        }
        else
        {
            deadline.async_wait(std::move(fun));
        }
    }

private:
    DECLARE_LOGGER("DtlAsioClient");

    ba::io_service& io_service_;
    ba::ip::tcp::socket socket_;
    std::unique_ptr<ba::io_service::strand> strand_;
    std::atomic<uint64_t> tag_;
    // only used for logging - get rid of it?
    be::Namespace nspace_;
    TunnelCapnProtoHeader header_;

    // ba::async_write operations must not overlap as they might consist of
    // multiple async_write_some invocations on a given stream (socket), so
    // requests need to be queued up until the previous request is sent.
    // * no locking - access only from within strand_
    // * push back / pop front
    // TODO: consider merging adjacent writes?
    std::list<RequestInfoPtr> tx_queue_;
    long sending_;

    // protects the map
    mutable boost::mutex mutex_;

    using RequestInfoMap = std::unordered_map<uint64_t, RequestInfoPtr>;
    RequestInfoMap requests_;

    RequestInfoPtr
    unlink_request_(uint64_t tag);

    void
    cancel_request_(uint64_t tag,
                    const std::exception& ex = RequestCancelledException("Request cancelled",
                                                                         "DtlClient",
                                                                         ECANCELED)) noexcept;

    void
    send_one_();

    void
    recv_header_();

    void
    recv_response_();

    void
    flush_(const uint64_t tag) noexcept
    {
        int val = 0;
        int ret = setsockopt(socket_.native_handle(),
                             IPPROTO_TCP,
                             TCP_CORK,
                             &val,
                             sizeof(val));
        if (ret == 0)
        {
            val = 1;
            ret = setsockopt(socket_.native_handle(),
                             IPPROTO_TCP,
                             TCP_CORK,
                             &val,
                             sizeof(val));
        }

        if (ret)
        {
            ret = errno;
            LOG_ERROR(nspace_ << ": failed to flush: " << strerror(errno));
            cancel_request_(tag,
                            fungi::IOException("failed to flush DTL client request",
                                               nspace_.str().c_str(),
                                               ret));
        }
    }

    template<typename Ex>
    void
    shutdown_(const Ex&) noexcept;

    void
    shutdown_(const bs::error_code& ec) noexcept
    {
        shutdown_(bs::system_error(ec));
    }

    // all these stranded_* helpers could be unified into a template (or two)
    // that also passes in the method / free function to invoke but async_connect,
    // async_write and async_read are templates themselves and specifying their
    // template arguments quickly becomes unwieldy.
    template<typename Fun>
    void
    stranded_async_connect_(decltype(socket_)::endpoint_type& ep,
                            Fun&& fun)
    {
        if (strand_)
        {
            socket_.async_connect(ep,
                                  strand_->wrap(std::move(fun)));
        }
        else
        {
            socket_.async_connect(ep,
                                  std::move(fun));
        }
    }

    template<typename Bufs,
             typename Fun>
    void
    stranded_async_read_(Bufs&& bufs,
                         Fun&& fun)
    {
        if (strand_)
        {
            ASSERT(strand_->running_in_this_thread());

            ba::async_read(socket_,
                           std::move(bufs),
                           strand_->wrap(std::move(fun)));
        }
        else
        {
            ba::async_read(socket_,
                           std::move(bufs),
                           std::move(fun));
        }
    }

    template<typename Bufs,
             typename Fun>
    void
    stranded_async_write_(Bufs&& bufs,
                          Fun&& fun)
    {
        if (strand_)
        {
            ASSERT(strand_->running_in_this_thread());

            ba::async_write(socket_,
                            std::move(bufs),
                            strand_->wrap(std::move(fun)));
        }
        else
        {
            ba::async_write(socket_,
                            std::move(bufs),
                            std::move(fun));
        }
    }
};

namespace
{

using Clock = std::chrono::steady_clock;

class RequestInfo
    : public std::enable_shared_from_this<RequestInfo>
{
    size_t
    count_tx_buffers_() const
    {
        size_t count = 2; // header_ + capnp_msg_

        struct SegmentsVisitor
            : public boost::static_visitor<size_t>
        {
            size_t
            operator()(const std::vector<FailOverCacheEntry>& vec) const
            {
                return vec.size();
            }

            size_t
            operator()(const CapnProtoClient::RawBufDesc& desc) const
            {
                VERIFY(desc.first or not desc.second);
                return desc.first ? 1 : 0;
            }
        };

        SegmentsVisitor v;
        count += boost::apply_visitor(v,
                                      bulk_data_);

        return count;
    }

    void
    setup_tx_buffers_()
    {
        tx_buffers_[0] = ba::buffer(&header_,
                                    sizeof(header_));
        kj::ArrayPtr<capnp::byte> b(capnp_msg_.asBytes());
        tx_buffers_[1] = ba::buffer(b.begin(),
                                    b.size());

        struct SegmentsVisitor
            : public boost::static_visitor<size_t>
        {
            std::vector<ba::const_buffer>& tx_buffers_;

            SegmentsVisitor(std::vector<ba::const_buffer>& tx_buffers)
                : tx_buffers_(tx_buffers)
            {};

            size_t
            operator()(const std::vector<FailOverCacheEntry>& vec) const
            {
                size_t res = 0;

                for (size_t i = 0; i < vec.size(); ++i)
                {
                    const auto& e = vec[i];

                    VERIFY(e.buffer_);
                    VERIFY(tx_buffers_.size() > i + 2);
                    tx_buffers_[i + 2] = ba::buffer(e.buffer_,
                                                    e.size_);
                    res += e.size_;
                }

                return res;
            }

            size_t
            operator()(const CapnProtoClient::RawBufDesc& desc) const
            {
                VERIFY(desc.first or not desc.second);
                if (desc.first)
                {
                    tx_buffers_.emplace_back(desc.first,
                                             desc.second);
                }
                return desc.second;
            }
        };

        SegmentsVisitor v(tx_buffers_);
        header_.data_size = boost::apply_visitor(v,
                                                 bulk_data_);
    }

public:
    RequestInfo(uint64_t tag,
                kj::Array<capnp::word> capnp_msg,
                CapnProtoClient::RawBufOrEntryVec bulk_data,
                std::shared_ptr<CapnProtoClient::AsioClient> ac)
        : header_(tag,
                  capnp_msg.size() * sizeof(capnp::word))
        , capnp_msg_(std::move(capnp_msg))
        , bulk_data_(std::move(bulk_data))
        , tx_buffers_(count_tx_buffers_())
        , deadline_(ac->io_service())
        , asio_client_(ac)
    {
        ASSERT(tx_buffers_.size() >= 2);
        setup_tx_buffers_();
    }

    ~RequestInfo() = default;

    RequestInfo(const RequestInfo&) = delete;

    RequestInfo&
    operator=(const RequestInfo&) = delete;

    uint64_t
    tag() const
    {
        return header_.tag;
    }

    void
    disable_deadline() noexcept
    {
        try
        {
            deadline_.expires_at(Clock::time_point::max());
        }
        CATCH_STD_ALL_LOG_IGNORE("failed to disable deadline of " << header_.tag);
    }

    void
    start_deadline(const ClientInterface::MaybeMilliSeconds& timeout)
    {
        if (timeout)
        {
            deadline_.expires_from_now(std::chrono::milliseconds(timeout->count()));
            check_deadline_();
        }
    }

    template<typename Ex>
    void
    cancel(const Ex& ex) noexcept
    {
        try
        {
            promise_.set_exception(ex);
        }
        CATCH_STD_ALL_LOG_IGNORE("failed to set exception of request " << header_.tag);

        disable_deadline();
    }

    void
    cancel() noexcept
    {
        cancel(RequestCancelledException("Request cancelled",
                                         "DtlClient",
                                         ECANCELED));
    }

    void
    complete(kj::Array<capnp::word> capnp_buf,
             BulkData data_buf) noexcept
    {
        try
        {
            promise_.set_value(std::make_pair(std::move(capnp_buf),
                                              std::move(data_buf)));
        }
        CATCH_STD_ALL_LOG_IGNORE("failed to set exception of request " << header_.tag);

        disable_deadline();
    }

    const std::vector<ba::const_buffer>&
    tx_buffers() const
    {
        return tx_buffers_;
    }

    RequestResultFuture
    future()
    {
        return promise_.get_future();
    }

private:
    DECLARE_LOGGER("DtlClientRequestInfo");

    TunnelCapnProtoHeader header_;
    kj::Array<capnp::word> capnp_msg_;
    CapnProtoClient::RawBufOrEntryVec bulk_data_;
    std::vector<ba::const_buffer> tx_buffers_;
    ba::steady_timer deadline_;
    std::shared_ptr<CapnProtoClient::AsioClient> asio_client_;
    boost::promise<RequestResult> promise_;

    void
    check_deadline_()
    {
        if (deadline_.expires_at() <= Clock::now())
        {
            LOG_ERROR("Request timeout, cancelling request " << header_.tag);
            cancel(RequestTimeoutException("Request timeout",
                                           "DtlClient",
                                           ETIMEDOUT));
        }
        else
        {
            asio_client_->post([self = shared_from_this(),
                                this]()
                               {
                                   auto f([self,
                                           this](const bs::error_code& ec)
                                          {
                                              if (ec)
                                              {
                                                  if (ec != boost::system::errc::operation_canceled)
                                                  {
                                                      cancel(bs::system_error(ec));
                                                  }
                                              }
                                              else
                                              {
                                                  // timer fired
                                                  check_deadline_();
                                              }
                                          });

                                   asio_client_->stranded_async_wait(deadline_,
                                                                     std::move(f));
                               });
        }
    }
};

}

#define LOCK()                                          \
    boost::lock_guard<decltype(mutex_)> g(mutex_)

CapnProtoClient::AsioClient::AsioClient(ba::io_service& io_service,
                                        const yt::ImplicitStrand implicit_strand,
                                        const be::Namespace& ns)
    : io_service_(io_service)
    , socket_(io_service_)
    , strand_(implicit_strand == yt::ImplicitStrand::T ?
              nullptr :
              std::make_unique<ba::io_service::strand>(io_service_))
    , tag_(yt::SourceOfUncertainty()(std::numeric_limits<uint64_t>::max()))
    , nspace_(ns)
    , sending_(0)
{}

CapnProtoClient::AsioClient::~AsioClient()
{
    LOCK();
    ASSERT(requests_.empty());
    for (auto& p : requests_)
    {
        p.second->cancel();
    }
}

void
CapnProtoClient::AsioClient::connect(const std::string& host,
                                     uint16_t port,
                                     const ClientInterface::MaybeMilliSeconds& timeout)
{
    decltype(socket_)::endpoint_type ep(ba::ip::address::from_string(host),
                                        port);

    boost::asio::steady_timer deadline(io_service_);
    bool connected = false;

    boost::promise<void> timeout_promise;
    boost::future<void> timeout_future;

    if (timeout)
    {
        timeout_future = timeout_promise.get_future();

        auto tfun([&,
                   self = shared_from_this()](const bs::error_code& ec)
                  {
                      if (ec)
                      {
                          if (ec != bs::errc::operation_canceled)
                          {
                              LOG_ERROR(nspace_ << ": connect timer error, " <<
                                        host << ":" << port << ": " << ec.message());
                          }
                      }
                      else
                      {
                          LOG_ERROR(nspace_ << ": timeout trying to connect to " <<
                                    host << ":" << port);
                      }

                      if (not connected)
                      {
                          try
                          {
                              socket_.close();
                          }
                          CATCH_STD_ALL_LOG_IGNORE(nspace_ << ": failed to close socket");

                          try
                          {
                              if (ec)
                              {
                                  timeout_promise.set_exception(bs::system_error(ec));
                              }
                              else
                              {
                                  timeout_promise.set_exception(ConnectTimeoutException("connect timeout",
                                                                                        "DtlClient",
                                                                                        ETIMEDOUT));
                              }
                          }
                          CATCH_STD_ALL_LOG_IGNORE(nspace_ << ": failed to set exception");
                      }
                      else
                      {
                          timeout_promise.set_value();
                      }
                  });

        deadline.expires_from_now(std::chrono::milliseconds(timeout->count()));
        stranded_async_wait(deadline,
                            std::move(tfun));
    }
    else
    {
        timeout_future = boost::make_ready_future();
    }

    {
        auto on_exit(yt::make_scope_exit([&]
                                         {
                                             const bool exc = std::uncaught_exception();

                                             try
                                             {
                                                 timeout_future.get();
                                             }
                                             catch (...)
                                             {
                                                 if (not exc)
                                                 {
                                                     throw;
                                                 }
                                             }
                                         }));

        boost::promise<void> connect_promise;
        boost::future<void> connect_future(connect_promise.get_future());

        auto cfun([&,
                   self = shared_from_this()](const bs::error_code& ec)
                  {
                      deadline.expires_at(Clock::time_point::max());

                      if (ec)
                      {
                          try
                          {
                              connect_promise.set_exception(bs::system_error(ec));
                          }
                          CATCH_STD_ALL_LOG_IGNORE(nspace_ << ": failed to set exception");
                      }
                      else
                      {
                          connected = true;
                          connect_promise.set_value();
                      }
                  });

        stranded_async_connect_(ep,
                                std::move(cfun));

        connect_future.get();
    }

    post(boost::bind(&CapnProtoClient::AsioClient::recv_header_,
                     shared_from_this()));
}

void
CapnProtoClient::AsioClient::recv_header_()
{
    auto fun([self = shared_from_this(),
              this](const bs::error_code& ec,
                    size_t bytes_transferred)
             {
                 if (ec)
                 {
                     if (ec == ba::error::eof)
                     {
                         LOG_INFO(nspace_ << ": " << ec.message() <<
                                  " - other side of connection was closed?");
                     }
                     else
                     {
                         LOG_ERROR(nspace_ << ": error receiving header: " << ec.message());
                     }

                     shutdown_(ec);
                 }
                 else
                 {
                     VERIFY(bytes_transferred == sizeof(header_));

                     if (header_.opcode != FailOverCacheCommand::TunnelCapnProto)
                     {
                         LOG_ERROR(nspace_ << ": got unexpected opcode " << header_.opcode);
                         shutdown_(ProtocolException("Unexpected opcode"));
                     }
                     else if (header_.capnp_size == 0)
                     {
                         LOG_ERROR(nspace_ << ": got unexpected response size 0");
                         shutdown_(ProtocolException("Response size 0"));
                     }
                     else
                     {
                         recv_response_();
                     }
                 }
             });

    stranded_async_read_(ba::buffer(&header_,
                                    sizeof(header_)),
                         std::move(fun));
}

void
CapnProtoClient::AsioClient::recv_response_()
{
    auto capnp_buf(std::make_shared<kj::Array<capnp::word>>());
    {
        auto tmp(kj::heapArray<capnp::word>(header_.capnp_size / sizeof(capnp::word)));
        *capnp_buf = std::move(tmp);
    }

    auto data_buf(std::make_shared<BulkData>(header_.data_size));

    auto fun([capnp_buf,
              data_buf,
              self = shared_from_this(),
              this](const bs::error_code& ec,
                    size_t bytes_transferred)
             {
                 if (ec)
                 {
                     LOG_ERROR(nspace_ << ": error receiving response: " << ec.message());
                     shutdown_(ec);
                 }
                 else
                 {
                     VERIFY(bytes_transferred == header_.capnp_size + header_.data_size);

                     RequestInfoPtr info(unlink_request_(header_.tag));
                     if (info)
                     {
                         info->complete(std::move(*capnp_buf),
                                        std::move(*data_buf));
                     }

                     recv_header_();
                 }
             });

    std::vector<ba::mutable_buffer> bufs(header_.data_size == 0 ? 1 : 2);
    kj::ArrayPtr<capnp::byte> capnp_array(capnp_buf->asBytes());
    bufs[0] = ba::buffer(capnp_array.begin(),
                         capnp_array.size());
    if (data_buf->size())
    {
        bufs[1] = ba::buffer(*data_buf);
    }

    stranded_async_read_(bufs,
                         std::move(fun));
}

template<typename Ex>
void
CapnProtoClient::AsioClient::shutdown_(const Ex& ex) noexcept
{
    ASSERT(not strand_ or strand_->running_in_this_thread());

    LOCK();

    for (auto& p : requests_)
    {
        p.second->cancel(ex);
    }

    requests_.clear();

    try
    {
        socket_.close();
    }
    CATCH_STD_ALL_LOG_IGNORE(nspace_ << ": failed to close socket");
}

RequestResultFuture
CapnProtoClient::AsioClient::send_request(kj::Array<capnp::word> capnp_msg,
                                          RawBufOrEntryVec bulk_data,
                                          const ClientInterface::MaybeMilliSeconds& timeout)
{
    const uint64_t tag = tag_++;

    auto info(std::make_shared<RequestInfo>(tag,
                                            std::move(capnp_msg),
                                            std::move(bulk_data),
                                            shared_from_this()));

    {
        LOCK();
        bool ok = false;
        std::tie(std::ignore, ok) = requests_.emplace(tag,
                                                      info);
        VERIFY(ok);
    }

    post([info,
          self = shared_from_this(),
          this]()
         {
             tx_queue_.push_back(info);
             if (sending_ == 0)
             {
                 send_one_();
             }
         });
    info->start_deadline(timeout);
    return info->future();
}

void
CapnProtoClient::AsioClient::send_one_()
{
    VERIFY(sending_ == 0);
    ++sending_;

    VERIFY(not tx_queue_.empty());
    RequestInfoPtr info(tx_queue_.front());
    tx_queue_.pop_front();

    auto fun([info,
              self = shared_from_this(),
              this](const bs::error_code& ec,
                    size_t bytes_transferred)
             {
                 --sending_;
                 VERIFY(sending_ == 0);

                 if (ec)
                 {
                     LOG_ERROR(nspace_ << ": async write returned error " <<
                               ec.message() << " for tag " << info->tag());
                     cancel_request_(info->tag(),
                                     bs::system_error(ec));
                 }
                 else
                 {
                     VERIFY(bytes_transferred == ba::buffer_size(info->tx_buffers()));
                     flush_(info->tag());
                 }

                 if (not tx_queue_.empty())
                 {
                     send_one_();
                 }
             });

    stranded_async_write_(info->tx_buffers(),
                          std::move(fun));
}

RequestInfoPtr
CapnProtoClient::AsioClient::unlink_request_(uint64_t tag)
{
    RequestInfoPtr info;

    {
        LOCK();
        auto it = requests_.find(tag);
        if (it != requests_.end())
        {
            info = it->second;
            requests_.erase(it);
        }
    }

    return info;
}

void
CapnProtoClient::AsioClient::cancel_request_(uint64_t tag,
                                             const std::exception& ex) noexcept
{
    ASSERT(not strand_ or strand_->running_in_this_thread());

    tx_queue_.remove_if([tag](const RequestInfoPtr& info) -> bool
                        {
                            return info->tag() == tag;
                        });

    RequestInfoPtr info(unlink_request_(tag));
    if (info)
    {
        info->cancel(ex);
    }
}

#undef LOCK

CapnProtoClient::CapnProtoClient(const FailOverCacheConfig& cfg,
                                 const yt::ImplicitStrand implicit_strand,
                                 const OwnerTag owner_tag,
                                 ba::io_service& io_service,
                                 const be::Namespace& nspace,
                                 const LBASize lba_size,
                                 const ClusterMultiplier cmult,
                                 const ClientInterface::MaybeMilliSeconds& request_timeout,
                                 const ClientInterface::MaybeMilliSeconds& connect_timeout,
                                 const size_t burst_size)
    : ClientInterface(cfg,
                      nspace,
                      lba_size,
                      cmult,
                      request_timeout,
                      connect_timeout)
    , asio_client_(std::make_shared<AsioClient>(io_service,
                                                implicit_strand,
                                                nspace))
    , owner_tag_(owner_tag)
    , burst_size_(burst_size)
{
    asio_client_->connect(cfg.host,
                          cfg.port,
                          connect_timeout);
    open_();
}

CapnProtoClient::~CapnProtoClient()
{
    if (delete_failover_dir_)
    {
        LOG_INFO(nspace_ << ": closing session / removing data");

        try
        {
            close_();
        }
        CATCH_STD_ALL_LOG_IGNORE(nspace_ << ": failed to close session");
    }
    else
    {
        LOG_INFO(nspace_ << ": not closing session / removing data");
    }

    asio_client_->shutdown(RequestCancelledException("client shutting down",
                                                     "DtlClient",
                                                     ECANCELED));
}

namespace
{

DECLARE_LOGGER("CapnProtoClientUtils");

void
expect_no_bulk_data(const BulkData& bulk_data)
{
    if (not bulk_data.empty())
    {
        LOG_ERROR("unexpected bulk data of size " << bulk_data.size());
        throw ProtocolException("unexpected bulk data received in DTL response");
    }
}

}

template<typename Rsp,
         typename Fun>
boost::future<void>
CapnProtoClient::submit_(capnp::MessageBuilder& mb,
                         RawBufOrEntryVec bulk_data,
                         typename Rsp::Reader (proto::ResponseData::Reader::*get_reader)() const,
                         Fun&& fun)
{

    return asio_client_->send_request(capnp::messageToFlatArray(mb),
                                      std::move(bulk_data),
                                      request_timeout_)
        .then(yt::InlineExecutor::get(),
              [fun = std::move(fun),
               get_reader](RequestResultFuture f) -> void
              {
                  RequestResult res(f.get());
                  capnp::FlatArrayMessageReader mreader(std::move(res.first));
                  BulkData bulk_data(std::move(res.second));

                  const auto root(mreader.getRoot<proto::Response>());
                  const proto::Response::Rsp::Reader rreader(root.getRsp());

                  switch (rreader.which())
                  {
                  case proto::Response::Rsp::Which::OK:
                      {
                          const proto::ResponseData::Reader r(rreader.getOk());
                          fun((r.*get_reader)(),
                              std::move(bulk_data));
                          break;
                      }
                  case proto::Response::Rsp::Which::ERROR:
                      {
                          expect_no_bulk_data(bulk_data);
                          const proto::Error::Reader ereader(rreader.getError());
                          LOG_ERROR("DTL reported error: " <<
                                    ereader.getMessage().cStr() << ", code " <<
                                    ereader.getCode());
                          throw ServerException(ereader.getMessage().cStr(),
                                                "DtlServer",
                                                ereader.getCode());
                          break;
                      }
                  }
              });
}

boost::future<void>
CapnProtoClient::addEntries(std::vector<FailOverCacheEntry> entries)
{
    capnp::MallocMessageBuilder mbuilder;

    auto root(mbuilder.initRoot<proto::Request>());
    proto::RequestData::Builder rbuilder(root.initRequestData());
    proto::AddEntriesRequest::Builder builder(rbuilder.initAddEntriesRequest());
    capnp::List<proto::ClusterEntry>::Builder lbuilder(builder.initEntries(entries.size()));

    for (size_t i = 0; i < entries.size(); ++i)
    {
        auto e = lbuilder[i];
        e.setAddress(entries[i].lba_);
        e.setClusterLocation(reinterpret_cast<const uint64_t&>(entries[i].cli_));
    }

    return submit_<proto::AddEntriesResponse>(mbuilder,
                                              std::move(entries),
                                              &proto::ResponseData::Reader::getAddEntriesResponse,
                                              [this](const proto::AddEntriesResponse::Reader&,
                                                 BulkData bulk_data)
                                              {
                                                  expect_no_bulk_data(bulk_data);
                                              });
}

boost::future<void>
CapnProtoClient::addEntries(const std::vector<ClusterLocation>& locs,
                            uint64_t addr,
                            const uint8_t* buf)
{
    capnp::MallocMessageBuilder mbuilder;
    auto root(mbuilder.initRoot<proto::Request>());
    proto::RequestData::Builder rbuilder(root.initRequestData());
    proto::AddEntriesRequest::Builder builder(rbuilder.initAddEntriesRequest());
    capnp::List<proto::ClusterEntry>::Builder lbuilder(builder.initEntries(locs.size()));

    for (size_t i = 0; i < locs.size(); ++i)
    {
        auto e = lbuilder[i];
        e.setAddress(addr + i * cluster_multiplier());
        e.setClusterLocation(*reinterpret_cast<const uint64_t*>(&locs[i]));
    }

    return submit_<proto::AddEntriesResponse>(mbuilder,
                                              std::make_pair(buf,
                                                             cluster_multiplier() * lba_size() * locs.size()),
                                              &proto::ResponseData::Reader::getAddEntriesResponse,
                                              [](const proto::AddEntriesResponse::Reader&,
                                                 BulkData bulk_data)
                                              {
                                                  expect_no_bulk_data(bulk_data);
                                              });

}

boost::future<void>
CapnProtoClient::flush()
{
    capnp::MallocMessageBuilder mbuilder;
    auto root(mbuilder.initRoot<proto::Request>());
    proto::RequestData::Builder rbuilder(root.initRequestData());
    proto::FlushRequest::Builder builder(rbuilder.initFlushRequest());

    return submit_<proto::FlushResponse>(mbuilder,
                                         std::make_pair(nullptr, 0),
                                         &proto::ResponseData::Reader::getFlushResponse,
                                         [](const proto::FlushResponse::Reader&,
                                            BulkData bulk_data)
                                         {
                                             expect_no_bulk_data(bulk_data);
                                         });
}

void
CapnProtoClient::clear()
{
    capnp::MallocMessageBuilder mbuilder;
    auto root(mbuilder.initRoot<proto::Request>());
    proto::RequestData::Builder rbuilder(root.initRequestData());
    proto::ClearRequest::Builder builder(rbuilder.initClearRequest());

    submit_<proto::ClearResponse>(mbuilder,
                                  std::make_pair(nullptr, 0),
                                  &proto::ResponseData::Reader::getClearResponse,
                                  [](const proto::ClearResponse::Reader&,
                                     BulkData bulk_data)
                                  {
                                      expect_no_bulk_data(bulk_data);
                                  }).get();
}

void
CapnProtoClient::removeUpTo(const SCO sco) noexcept
{
    try
    {
        capnp::MallocMessageBuilder mbuilder;
        auto root(mbuilder.initRoot<proto::Request>());
        proto::RequestData::Builder rbuilder(root.initRequestData());
        proto::RemoveUpToRequest::Builder builder(rbuilder.initRemoveUpToRequest());
        const ClusterLocation loc(sco,
                                  std::numeric_limits<SCOOffset>::max());
        builder.setClusterLocation(reinterpret_cast<const uint64_t&>(loc));

        submit_<proto::RemoveUpToResponse>(mbuilder,
                                           std::make_pair(nullptr, 0),
                                           &proto::ResponseData::Reader::getRemoveUpToResponse,
                                           [](const proto::RemoveUpToResponse::Reader&,
                                              BulkData bulk_data)
                                           {
                                               expect_no_bulk_data(bulk_data);
                                           }).get();
    }
    CATCH_STD_ALL_LOG_IGNORE(nspace_ << ": failed to remove up to " << sco);
}

void
CapnProtoClient::getSCORange(SCO& begin,
                             SCO& end)
{
    ClusterLocation b;
    ClusterLocation e;
    std::tie(b, e) = getRange();
    begin = b.sco();
    end = e.sco();
}

std::pair<ClusterLocation, ClusterLocation>
CapnProtoClient::getRange()
{
    capnp::MallocMessageBuilder mbuilder;
    auto root(mbuilder.initRoot<proto::Request>());
    proto::RequestData::Builder rbuilder(root.initRequestData());
    proto::GetRangeRequest::Builder builder(rbuilder.initGetRangeRequest());

    ClusterLocation begin(0);
    ClusterLocation end(0);

    submit_<proto::GetRangeResponse>(mbuilder,
                                     std::make_pair(nullptr, 0),
                                     &proto::ResponseData::Reader::getGetRangeResponse,
                                     [&begin,
                                      &end](const proto::GetRangeResponse::Reader& reader,
                                            BulkData bulk_data)
                                     {
                                         expect_no_bulk_data(bulk_data);

                                         const uint64_t b = reader.getBegin();
                                         const uint64_t e = reader.getEnd();
                                         begin = reinterpret_cast<const ClusterLocation&>(b);
                                         end = reinterpret_cast<const ClusterLocation&>(e);
                                     }).get();

    return std::make_pair(begin, end);
}

void
CapnProtoClient::open_()
{
    capnp::MallocMessageBuilder mbuilder;
    auto root(mbuilder.initRoot<proto::Request>());
    proto::RequestData::Builder rbuilder(root.initRequestData());
    proto::OpenRequest::Builder builder(rbuilder.initOpenRequest());

    builder.setNspace(capnp::Text::Reader(nspace_.str()));
    builder.setClusterSize(lba_size() * cluster_multiplier());
    builder.setOwnerTag(owner_tag_.t);

    submit_<proto::OpenResponse>(mbuilder,
                                 std::make_pair(nullptr, 0),
                                 &proto::ResponseData::Reader::getOpenResponse,
                                 [](const proto::OpenResponse::Reader&,
                                    BulkData bulk_data)
                                 {
                                     expect_no_bulk_data(bulk_data);
                                 }).get();
}

void
CapnProtoClient::close_()
{
    capnp::MallocMessageBuilder mbuilder;
    auto root(mbuilder.initRoot<proto::Request>());
    proto::RequestData::Builder rbuilder(root.initRequestData());
    proto::CloseRequest::Builder builder(rbuilder.initCloseRequest());

    submit_<proto::CloseResponse>(mbuilder,
                                  std::make_pair(nullptr, 0),
                                  &proto::ResponseData::Reader::getCloseResponse,
                                  [](const proto::CloseResponse::Reader&,
                                     BulkData bulk_data)
                                  {
                                      expect_no_bulk_data(bulk_data);
                                  }).get();
}

template<typename Fun>
size_t
CapnProtoClient::get_entries_(ClusterLocation loc,
                              size_t num_entries,
                              Fun&& fun)
{
    capnp::MallocMessageBuilder mbuilder;
    auto root(mbuilder.initRoot<proto::Request>());
    proto::RequestData::Builder rbuilder(root.initRequestData());
    proto::GetEntriesRequest::Builder builder(rbuilder.initGetEntriesRequest());
    builder.setStartClusterLocation(reinterpret_cast<const uint64_t&>(loc));
    builder.setNumEntries(num_entries);

    size_t ret = 0;
    const size_t csize = lba_size() * cluster_multiplier();

    submit_<proto::GetEntriesResponse>(mbuilder,
                                       std::make_pair(nullptr, 0),
                                       &proto::ResponseData::Reader::getGetEntriesResponse,
                                       [&](const proto::GetEntriesResponse::Reader& reader,
                                           BulkData bulk_data)
                                       {
                                           const capnp::List<proto::ClusterEntry>::Reader
                                               lreader(reader.getEntries());

                                           size_t off = 0;
                                           for (const auto& r : lreader)
                                           {
                                               ClusterLocation cl;
                                               reinterpret_cast<uint64_t&>(cl) = r.getClusterLocation();


                                               VERIFY(off + csize <= bulk_data.size());

                                               fun(cl,
                                                   Lba(r.getAddress()),
                                                   bulk_data.data() + off,
                                                   csize);

                                               off += csize;
                                               ++ret;
                                           }
                                       }).get();

    return ret;
}

uint64_t
CapnProtoClient::getSCOFromFailOver(SCO sco,
                                    SCOProcessorFun fun)
{
    uint64_t size = 0;
    ClusterLocation loc(sco, 0);

    bool skip_first = false;
    size_t nents = 0;

    auto fn([&](ClusterLocation cl,
                Lba lba,
                const uint8_t* buf,
                size_t bufsize)
            {
                if (cl == loc and skip_first)
                {
                    return;
                }

                if (cl.sco() == sco)
                {
                    fun(cl,
                        lba,
                        buf,
                        bufsize);

                    size += cluster_multiplier() * lba_size();
                    loc = cl;
                    skip_first = true;
                    ++nents;
                }
            });

    do
    {
        nents = 0;
        get_entries_(loc,
                     burst_size_,
                     fn);
    }
    while (nents > 0 and loc.sco() == sco);

    return size;
}

void
CapnProtoClient::getEntries(SCOProcessorFun fun)
{
    ClusterLocation loc(0);
    bool skip_first = false;
    size_t nents = 0;

    auto fn([&](ClusterLocation cl,
                Lba lba,
                const uint8_t* buf,
                size_t bufsize)
            {
                if (cl == loc and skip_first)
                {
                    return;
                }

                fun(cl,
                    lba,
                    buf,
                    bufsize);

                ++nents;
                loc = cl;
                skip_first = true;
            });

    do
    {
        nents = 0;
        get_entries_(loc,
                     burst_size_,
                     fn);
    }
    while (nents > 0);
}

}

}
