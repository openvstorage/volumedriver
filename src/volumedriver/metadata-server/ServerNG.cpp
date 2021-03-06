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

#include "Protocol.h"
#include "ServerNG.h"
#include "Utils.h"

#include <map>

#include <capnp/message.h>
#include <capnp/serialize.h>

#include <youtils/Assert.h>
#include <youtils/SharedMemoryRegion.h>

namespace metadata_server
{

#define LOCK()                                  \
    boost::lock_guard<decltype(lock_)> lg__(lock_)

namespace ba = boost::asio;
namespace bi = boost::interprocess;
namespace mdsproto = metadata_server_protocol;
namespace yt = youtils;
namespace vd = volumedriver;

struct ServerNG::ConnectionState
{
    // unordered_map seems to insist on a copy constructor!?
    std::map<yt::SharedMemoryRegionId,
             yt::SharedMemoryRegion> regions;

    yt::SharedMemoryRegion&
    get_region(yt::SharedMemoryRegionId id)
    {
        auto it = regions.find(id);
        if (it == regions.end())
        {
            auto res(regions.emplace(id,
                                     yt::SharedMemoryRegion(id)));
            VERIFY(res.second);
            it = res.first;
        }

        return it->second;
    }
};

ServerNG::ServerNG(DataBaseInterfacePtr db,
                   const std::string& addr,
                   const uint16_t port,
                   const boost::optional<std::chrono::seconds>& timeout,
                   const uint32_t nthreads)
    : timeout_(timeout)
    , db_(db)
    , server_(addr,
              port,
              [&](const std::shared_ptr<yt::LocORemUnixConnection>& c) mutable
              {
                  recv_header_(c,
                               std::make_shared<ConnectionState>());
              },
              [&](const std::shared_ptr<yt::LocORemTcpConnection>& c) mutable
              {
                  recv_header_(c,
                               std::make_shared<ConnectionState>());
              },
              nthreads)
    , stop_(false)
{
    try
    {
        for (size_t i = 0; i < nthreads; ++i)
        {
            threads_.create_thread(boost::bind(&ServerNG::work_,
                                               this));
        }
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("Failed to create worker pool: " << EWHAT);
            stop_work_();
            throw;
        });
}

ServerNG::~ServerNG()
{
    try
    {
        stop_work_();
    }
    CATCH_STD_ALL_LOG_IGNORE("Failed to stop");
}

void
ServerNG::stop_work_()
{
    {
        LOCK();
        stop_ = true;
        cond_.notify_all();
    }

    threads_.join_all();
}

void
ServerNG::work_()
{
    pthread_setname_np(pthread_self(), "mds_del_work");

    while (true)
    {
        try
        {
            DelayedFun fun;

            {
                boost::unique_lock<decltype(lock_)> u(lock_);
                cond_.wait(u,
                           [&]
                           {
                               return not delayed_work_.empty() or stop_;
                           });

                if (stop_)
                {
                    break;
                }

                if (not delayed_work_.empty())
                {
                    fun = std::move(delayed_work_.front());
                    delayed_work_.pop_front();
                }
            }

            if (fun)
            {
                fun();
            }
        }
        CATCH_STD_ALL_LOG_IGNORE("Caught exception while executing delayed work");
    }

    LOG_INFO("stopping delayed work thread");
}

template<typename C>
void
ServerNG::recv_header_(const std::shared_ptr<C>& conn,
                       ConnectionStatePtr state)
{
    TODO("AR: figure out how to use boost::asio strand::wrap with non-copyable callables and use a unique_ptr instead");

    // LOG_TRACE(&conn << ": scheduling task to read headers");

    auto h(std::make_shared<mdsproto::RequestHeader>());
    auto buf(ba::buffer(h.get(),
                        sizeof(*h)));

    auto fun([hdr = h, state, this](const std::shared_ptr<C>& c)
             {
                 if (hdr->magic != mdsproto::magic)
                 {
                     LOG_ERROR("Request lacks our protocol magic, giving up");
                     throw mdsproto::NoMagicException("Request lacks protocol magic");
                 }

                 // LOG_TRACE("got a new request header: " <<
                 //           hdr->request_type << ", tag " <<
                 //           hdr->tag << ", size " <<
                 //           hdr->size);

                 get_data_(c,
                           state,
                           hdr);
             });

    conn->async_read(buf,
                     std::move(fun),
                     boost::none);
}

template<typename C>
void
ServerNG::get_data_(const std::shared_ptr<C>& conn,
                    ConnectionStatePtr state,
                    const HeaderPtr& hdr)
{
    const yt::SharedMemoryRegionId& mr_id(hdr->out_region);

    if (hdr->size and mr_id != yt::SharedMemoryRegionId(0))
    {
        TODO("AR: verify that conn is indeed local!?");

        auto& mr = state->get_region(mr_id);

        auto src(std::make_shared<DataSource>(kj::arrayPtr(static_cast<const capnp::word*>(mr.address()) +
                                                           hdr->out_offset,
                                                           hdr->size / sizeof(capnp::word))));
        const auto& seg = boost::get<kj::ArrayPtr<const capnp::word>>(*src);
        auto reader(std::make_shared<capnp::SegmentArrayMessageReader>(kj::arrayPtr(&seg, 1)));

        dispatch_(conn,
                  state,
                  hdr,
                  src,
                  reader);
    }
    else
    {
        recv_data_(conn,
                   state,
                   hdr);
    }
}

template<typename C>
void
ServerNG::recv_data_(const std::shared_ptr<C>& conn,
                     ConnectionStatePtr state,
                     const HeaderPtr& h)
{
    // LOG_TRACE(&conn << ": scheduling task to read data");

    // shared_ptr, as moving a unique_ptr will stop at the asio strand
    auto data(std::make_shared<DataSource>(std::vector<uint8_t>(h->size)));
    auto& vec = boost::get<std::vector<uint8_t>>(*data);
    auto buf(ba::buffer(vec));

    auto fun([data, state, hdr = h, this]
             (const std::shared_ptr<C>& c)
             {
                 // LOG_TRACE(&c << ": got data for header " << hdr->tag);
                 auto& vec = boost::get<std::vector<uint8_t>>(*data);
                 auto rxdata(kj::arrayPtr(reinterpret_cast<const capnp::word*>(vec.data()),
                                          vec.size() / sizeof(capnp::word)));
                 auto reader(std::make_shared<capnp::FlatArrayMessageReader>(rxdata));

                 dispatch_(c,
                           state,
                           hdr,
                           data,
                           reader);
             });

    conn->async_read(buf,
                     std::move(fun),
                     timeout_);
}

template<typename C>
void
ServerNG::dispatch_(const std::shared_ptr<C>& conn,
                    ConnectionStatePtr state,
                    const HeaderPtr& hdr,
                    const DataSourcePtr& data,
                    const MessageReaderPtr& reader)
{
    // LOG_TRACE(&conn << ": dispatching request: " <<
    //           hdr.request_type << ", tag " <<
    //           hdr.tag << ", size " <<
    //           hdr.size << " (hdr)");

#define CASE(req_type, mem_fn, defer)                                   \
    case mdsproto::RequestHeader::Type::req_type:                       \
        handle_<mdsproto::RequestHeader::Type::req_type>(conn,          \
                                                         state,         \
                                                         hdr,           \
                                                         data,          \
                                                         defer,         \
                                                         reader,        \
                                                         &ServerNG::mem_fn); \
            return

    switch (hdr->request_type)
    {
        CASE(Open, open_, yt::DeferExecution::F);
        CASE(Drop, drop_, yt::DeferExecution::F);
        CASE(Clear, clear_, yt::DeferExecution::F);
        CASE(MultiGet, multiget_, yt::DeferExecution::F);
        CASE(MultiSet, multiset_, yt::DeferExecution::F);
        CASE(SetRole, set_role_, yt::DeferExecution::T);
        CASE(GetRole, get_role_, yt::DeferExecution::F);
        CASE(List, list_namespaces_, yt::DeferExecution::F);
        CASE(Ping, ping_, yt::DeferExecution::F);
        CASE(ApplyRelocationLogs, apply_relocation_logs_, yt::DeferExecution::T);
        CASE(CatchUp, catch_up_, yt::DeferExecution::T);
        CASE(GetTableCounters, get_table_counters_, yt::DeferExecution::F);
        CASE(GetOwnerTag, get_owner_tag_, yt::DeferExecution::F);
    }

#undef CASE

    LOG_ERROR(&conn << ": unknown request type " <<
              static_cast<uint32_t>(hdr->request_type) << " - stopping processing here");

    error_(conn,
           state,
           mdsproto::ResponseHeader::Type::UnknownRequest,
           hdr->tag,
           "unknown request");
}

template<enum mdsproto::RequestHeader::Type R,
         typename C,
         typename Traits>
void
ServerNG::handle_(const std::shared_ptr<C>& conn,
                  ConnectionStatePtr state,
                  const HeaderPtr& hdr,
                  const DataSourcePtr& data,
                  const yt::DeferExecution defer,
                  const MessageReaderPtr& reader,
                  void (ServerNG::*mem_fn)(typename Traits::Params::Reader& reader,
                                           typename Traits::Results::Builder& builder))
{
    // LOG_TRACE(&conn << ", " << hdr->request_type << ", tag " << hdr->tag);

    bool use_shmem = hdr->in_region != yt::SharedMemoryRegionId(0);
    if (use_shmem)
    {
        if (hdr->in_region == hdr->out_region and
            hdr->out_offset + hdr->size > hdr->in_offset)
        {
            LOG_WARN(&conn <<
                     ": cannot deal with overlapping shmem regions for input (off: " <<
                     hdr->out_offset << ", size " << hdr->size << ") and output (off: " <<
                     hdr->in_offset << ") - falling back to sending data inband");
            use_shmem = false;
        }
        else
        {
            try
            {
                handle_shmem_<R>(conn,
                                 state,
                                 hdr,
                                 data,
                                 defer,
                                 reader,
                                 mem_fn);
            }
            catch (kj::Exception& e)
            {
                LOG_ERROR("Failed to build shmem message " << e.getDescription().cStr() <<
                          " - falling back to socket");
                use_shmem = false;
            }
            catch (bi::interprocess_exception& e)
            {
                LOG_ERROR("Failed to build shmem message " << e.what() <<
                          " - falling back to socket");
                use_shmem = false;
            }
        }
    }

    if (not use_shmem)
    {
        LOG_TRACE(&conn << ": sending data inband");

        auto builder(std::make_shared<capnp::MallocMessageBuilder>());

        do_handle_<R>(conn,
                      state,
                      hdr,
                      data,
                      defer,
                      builder,
                      reader,
                      mem_fn);
    }
}

template<enum mdsproto::RequestHeader::Type R,
         typename C,
         typename Traits>
void
ServerNG::handle_shmem_(const std::shared_ptr<C>& conn,
                        ConnectionStatePtr state,
                        const HeaderPtr& hdr,
                        const DataSourcePtr& data,
                        const yt::DeferExecution defer,
                        const MessageReaderPtr& reader,
                        void (ServerNG::*mem_fn)(typename Traits::Params::Reader&,
                                                 typename Traits::Results::Builder&))
{
    // LOG_TRACE(&conn << ": trying shmem: region " <<
    //           hdr.in_region << ", offset " << hdr.in_offset);

    auto& mr(state->get_region(hdr->in_region));

    uint8_t* addr = static_cast<uint8_t*>(mr.address()) + hdr->in_offset;
    auto array(kj::arrayPtr(reinterpret_cast<capnp::word*>(addr),
                            (mr.size() - hdr->in_offset) / sizeof(capnp::word)));
    auto builder(std::make_shared<capnp::FlatMessageBuilder>(array));

    do_handle_<R>(conn,
                  state,
                  hdr,
                  data,
                  defer,
                  builder,
                  reader,
                  mem_fn);
}

namespace
{

mdsproto::ResponseHeader::Type
build_error(capnp::MessageBuilder& builder,
            mdsproto::ErrorType error_type,
            const char* msg)
{
    auto txroot(builder.initRoot<mdsproto::Error>());
    txroot.setMessage(msg);
    txroot.setErrorType(error_type);
    return mdsproto::ResponseHeader::Type::Error;
}

}

template<enum mdsproto::RequestHeader::Type R,
         typename C,
         typename Traits>
void
ServerNG::do_handle_(const std::shared_ptr<C>& conn,
                     ConnectionStatePtr state,
                     const HeaderPtr& hdr,
                     const DataSourcePtr& data,
                     const yt::DeferExecution defer,
                     const MessageBuilderPtr& builder,
                     const MessageReaderPtr& reader,
                     void (ServerNG::*mem_fn)(typename Traits::Params::Reader&,
                                              typename Traits::Results::Builder&))
{
    if (defer == yt::DeferExecution::T)
    {
        // keep the shared_ptrs alive
        auto fun([c = conn,
                  b = builder,
                  d = data,
                  f = mem_fn,
                  h = hdr,
                  r = reader,
                  s = state,
                  this]
                 {
                     do_handle_<R, C, Traits>(c,
                                              s,
                                              h,
                                              d,
                                              b,
                                              r,
                                              f);
                 });

        LOCK();
        delayed_work_.push_back(std::move(fun));
        cond_.notify_one();
    }
    else
    {
        do_handle_<R, C, Traits>(conn,
                                 state,
                                 hdr,
                                 data,
                                 builder,
                                 reader,
                                 mem_fn);
    }
}

template<enum mdsproto::RequestHeader::Type R,
         typename C,
         typename Traits>
void
ServerNG::do_handle_(const std::shared_ptr<C>& conn,
                     ConnectionStatePtr state,
                     const HeaderPtr& hdr,
                     const DataSourcePtr& /* data */,
                     const MessageBuilderPtr& builder,
                     const MessageReaderPtr& reader,
                     void (ServerNG::*mem_fn)(typename Traits::Params::Reader&,
                                              typename Traits::Results::Builder&))
{
    auto rxroot(reader->getRoot<typename Traits::Params>());

    mdsproto::ResponseHeader::Type rsp = mdsproto::ResponseHeader::Type::Ok;

    try
    {
        auto txroot(builder->initRoot<typename Traits::Results>());
        (this->*mem_fn)(rxroot,
                        txroot);
    }
    catch (kj::Exception& e)
    {
        LOG_ERROR("Failed to build message " << e.getDescription().cStr());
        throw;
    }
    catch (bi::interprocess_exception& e)
    {
        LOG_ERROR("Failed to build shmem message " << e.what());
        throw;
    }
    catch (vd::OwnerTagMismatchException& e)
    {
        LOG_ERROR(&conn << ": processing " << hdr->request_type <<
                  " caught exception " << e.what());
        rsp = build_error(*builder,
                          mdsproto::ErrorType::OWNER_TAG_MISMATCH,
                          e.what());
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR(&conn << ": processing " << hdr->request_type <<
                      " caught exception " << EWHAT);
            rsp = build_error(*builder,
                              mdsproto::ErrorType::UNKNOWN,
                              EWHAT);
        });

    send_response_(conn,
                   state,
                   rsp,
                   hdr->tag,
                   builder);
}

template<typename C>
void
ServerNG::error_(const std::shared_ptr<C>& conn,
                 ConnectionStatePtr state,
                 mdsproto::ResponseHeader::Type rsp,
                 mdsproto::Tag tag,
                 const std::string& msg)
{
    LOG_TRACE(&conn << ": error " << rsp << ", req tag " << tag << ", msg " << msg);

    auto builder(std::make_shared<capnp::MallocMessageBuilder>());
    auto txroot(builder->initRoot<typename mdsproto::Error>());

    txroot.setMessage(msg);

    send_response_(conn,
                   state,
                   rsp,
                   tag,
                   builder);
}

template<typename C>
void
ServerNG::send_response_(const std::shared_ptr<C>& conn,
                         ConnectionStatePtr state,
                         mdsproto::ResponseHeader::Type rsp,
                         mdsproto::Tag tag,
                         const MessageBuilderPtr& builder)
{
    auto fb_builder = std::dynamic_pointer_cast<capnp::FlatMessageBuilder>(builder);
    if (fb_builder != nullptr)
    {
        send_response_shmem_(conn,
                             state,
                             rsp,
                             tag,
                             fb_builder);
    }
    else
    {
        send_response_inband_(conn,
                              state,
                              rsp,
                              tag,
                              builder);
    }
}

template<typename C>
void
ServerNG::send_response_shmem_(const std::shared_ptr<C>& conn,
                               ConnectionStatePtr state,
                               mdsproto::ResponseHeader::Type rsp,
                               mdsproto::Tag tag,
                               const std::shared_ptr<capnp::FlatMessageBuilder>& builder)
{
    // LOG_TRACE(&conn << ": scheduling task to send response");

    const size_t size = capnp::computeSerializedSizeInWords(*builder) *
        sizeof(capnp::word);

    auto hdr(std::make_shared<const mdsproto::ResponseHeader>(rsp,
                                                              size,
                                                              tag,
                                                              mdsproto::ResponseHeader::UseShmem));

    // LOG_TRACE(&conn << " response: " << hdr->response_type << ", tag " <<
    //           hdr->tag << ", size " << hdr->size);

    auto fun([hdr,
              state,
              this](const std::shared_ptr<C>& c)
             {
                 // LOG_TRACE(&c << ": " << hdr->tag << " send completion");
                 recv_header_(c,
                              state);
             });

    conn->async_write(ba::buffer(hdr.get(),
                                 sizeof(*hdr)),
                      std::move(fun),
                      timeout_);
}

template<typename C>
void
ServerNG::send_response_inband_(const std::shared_ptr<C>& conn,
                                ConnectionStatePtr state,
                                mdsproto::ResponseHeader::Type rsp,
                                mdsproto::Tag tag,
                                const MessageBuilderPtr& builder)
{
    // LOG_TRACE(&conn << ": scheduling task to send response");

    auto data(std::make_shared<kj::Array<capnp::word>>(capnp::messageToFlatArray(*builder)));

    auto hdr(std::make_shared<const mdsproto::ResponseHeader>(rsp,
                                                              data->size() *
                                                              sizeof(capnp::word),
                                                              tag));

    const boost::array<ba::const_buffer, 2> bufs{{
            ba::buffer(&(*hdr),
                       sizeof(*hdr)),
                ba::buffer(data->begin(),
                           data->size() * sizeof(capnp::word))
                }};

    // LOG_TRACE(&conn << " response: " << hdr->response_type << ", tag " <<
    //           hdr->tag << ", size " << hdr->size);

    auto fun([data,
              hdr,
              state,
              this](const std::shared_ptr<C>& c)
             {
                 // LOG_TRACE(&c << ": " << hdr->tag << " send completion");
                 recv_header_(c,
                              state);
             });

    conn->async_write(bufs,
                      std::move(fun),
                      timeout_);
}

void
ServerNG::open_(mdsproto::Methods::OpenParams::Reader& reader,
                mdsproto::Methods::OpenResults::Builder&)
{
    const std::string nspace(reader.getNspace().begin(),
                             reader.getNspace().size());

    // LOG_TRACE("request to open " << nspace);
    db_->open(nspace);
}

void
ServerNG::drop_(mdsproto::Methods::DropParams::Reader& reader,
                mdsproto::Methods::DropResults::Builder&)
{
    const std::string nspace(reader.getNspace().begin(),
                             reader.getNspace().size());

    // LOG_TRACE("request to drop " << nspace);
    db_->drop(nspace);
}

void
ServerNG::clear_(mdsproto::Methods::ClearParams::Reader& reader,
                 mdsproto::Methods::ClearResults::Builder&)
{
    const std::string nspace(reader.getNspace().begin(),
                             reader.getNspace().size());
    const vd::OwnerTag owner_tag(reader.getOwnerTag());

    // LOG_TRACE("request to clear " << nspace);
    db_->open(nspace)->clear(owner_tag);
}

void
ServerNG::multiget_(mdsproto::Methods::MultiGetParams::Reader& reader,
                    mdsproto::Methods::MultiGetResults::Builder& builder)
{
    const std::string nspace(reader.getNspace().begin(),
                             reader.getNspace().size());

    auto keys_reader(reader.getKeys());
    TableInterface::Keys keys;
    keys.reserve(keys_reader.size());

    for (const auto& k : keys_reader)
    {
        keys.emplace_back(Key(reinterpret_cast<const char*>(k.begin()),
                              k.size()));
    }

    // LOG_TRACE("multiget request to " << nspace << ", size " << keys.size());

    auto vals(db_->open(nspace)->multiget(keys));

    size_t idx = 0;
    auto vals_builder(builder.initValues(keys.size()));

    for (const auto& v : vals)
    {
        if (v == boost::none)
        {
            capnp::Data::Reader r(nullptr, 0);
            vals_builder.set(idx, r);
        }
        else
        {
            capnp::Data::Reader r(reinterpret_cast<const uint8_t*>(v->c_str()),
                                  v->size());
            vals_builder.set(idx, r);
        }

        ++idx;
    }
}

void
ServerNG::multiset_(mdsproto::Methods::MultiSetParams::Reader& reader,
                    mdsproto::Methods::MultiSetResults::Builder&)
{
    const std::string nspace(reader.getNspace().begin(),
                             reader.getNspace().size());

    const Barrier barrier(reader.getBarrier() ?
                          Barrier::T :
                          Barrier::F);

    const vd::OwnerTag owner_tag(reader.getOwnerTag());

    auto recs_reader(reader.getRecords());

    TableInterface::Records recs;
    recs.reserve(recs_reader.size());

    for (const auto& r : recs_reader)
    {
        capnp::Data::Reader kreader(r.getKey());
        Key k(kreader.size() ? kreader.begin() : nullptr,
              kreader.size());

        capnp::Data::Reader vreader(r.getVal());
        Value v(vreader.size() ? vreader.begin() : nullptr,
                vreader.size());

        recs.emplace_back(Record(k, v));
    }

    // LOG_TRACE("multiset request to " << nspace << ", size " << recs.size());

    db_->open(nspace)->multiset(recs,
                                barrier,
                                owner_tag);
}

void
ServerNG::set_role_(mdsproto::Methods::SetRoleParams::Reader& reader,
                    mdsproto::Methods::SetRoleResults::Builder&)
{
    const std::string nspace(reader.getNspace().begin(),
                             reader.getNspace().size());
    const Role role(translate_role(reader.getRole()));
    const vd::OwnerTag owner_tag(reader.getOwnerTag());

    // LOG_TRACE("request to set role of " << nspace << " to " << role);
    db_->open(nspace)->set_role(role,
                                owner_tag);
}

void
ServerNG::get_role_(mdsproto::Methods::GetRoleParams::Reader& reader,
                    mdsproto::Methods::GetRoleResults::Builder& builder)
{
    const std::string nspace(reader.getNspace().begin(),
                             reader.getNspace().size());

    // LOG_TRACE("request to get role of " << nspace);

    const Role role(db_->open(nspace)->get_role());
    builder.setRole(translate_role(role));
}

void
ServerNG::list_namespaces_(mdsproto::Methods::ListParams::Reader&,
                           mdsproto::Methods::ListResults::Builder& builder)
{
    // LOG_TRACE("request to list namespaces");

    const std::vector<std::string> nspaces(db_->list_namespaces());

    size_t idx = 0;
    auto ns_builder(builder.initNspaces(nspaces.size()));

    for (const auto& n : nspaces)
    {
        capnp::Text::Reader r(n.c_str(),
                              n.size());
        ns_builder.set(idx, r);
        ++idx;
    }
}

void
ServerNG::ping_(mdsproto::Methods::PingParams::Reader& reader,
                mdsproto::Methods::PingResults::Builder& builder)
{
    auto data(reader.getData());

    // LOG_TRACE("ping request, " << data.size() << " bytes");

    capnp::Data::Reader r(data.begin(),
                          data.size());

    builder.setData(r);
}

void
ServerNG::apply_relocation_logs_(mdsproto::Methods::ApplyRelocationLogsParams::Reader& reader,
                                 mdsproto::Methods::ApplyRelocationLogsResults::Builder&)
{
    const std::string nspace(reader.getNspace().begin(),
                             reader.getNspace().size());
    const yt::UUID be_uuid(std::string(reader.getScrubId().begin(),
                                    reader.getScrubId().size()));
    const vd::ScrubId be_scrub_id(be_uuid);

    const auto md_uuid_str(std::string(reader.getMdScrubId().begin(),
                                       reader.getMdScrubId().size()));

    vd::MaybeScrubId md_scrub_id;
    if (not md_uuid_str.empty())
    {
        md_scrub_id = vd::ScrubId(yt::UUID(md_uuid_str));
    }

    const vd::SCOCloneID cid(reader.getCloneId());

    auto logs_reader(reader.getLogs());
    TableInterface::RelocationLogs logs;
    logs.reserve(logs_reader.size());

    for (const auto& l : logs_reader)
    {
        logs.emplace_back(l.begin(),
                          l.size());
    }

    // LOG_TRACE("apply_relocations request to " << nspace << ", size " << logs.size() <<
    //           ", scrub_id " << scrub_id << ", clone_id " << static_cast<uint32_t>(cid));

    db_->open(nspace)->apply_relocations(be_scrub_id,
                                         md_scrub_id,
                                         cid,
                                         logs);
}

void
ServerNG::catch_up_(mdsproto::Methods::CatchUpParams::Reader& reader,
                    mdsproto::Methods::CatchUpResults::Builder& builder)
{
    const std::string nspace(reader.getNspace().begin(),
                             reader.getNspace().size());

    const vd::DryRun dry_run(reader.getDryRun() ?
                             vd::DryRun::T :
                             vd::DryRun::F);
    const vd::CheckScrubId check_scrub_id(reader.getCheckScrubId() ?
                                          vd::CheckScrubId::T :
                                          vd::CheckScrubId::F);

    // LOG_TRACE("catch_up request to " << nspace << ", dry run: " << dry_run);

    const size_t num_tlogs = db_->open(nspace)->catch_up(dry_run,
                                                         check_scrub_id);

    builder.setNumTLogs(num_tlogs);
}

void
ServerNG::get_table_counters_(mdsproto::Methods::GetTableCountersParams::Reader& reader,
                              mdsproto::Methods::GetTableCountersResults::Builder& builder)
{
    const std::string nspace(reader.getNspace().begin(),
                             reader.getNspace().size());
    const vd::Reset reset(reader.getReset() ?
                          vd::Reset::T :
                          vd::Reset::F);

    const TableCounters table_counters = db_->open(nspace)->get_counters(reset);

    mdsproto::TableCounters::Builder cbuilder = builder.initCounters();
    cbuilder.setTotalTLogsRead(table_counters.total_tlogs_read);
    cbuilder.setIncrementalUpdates(table_counters.incremental_updates);
    cbuilder.setFullRebuilds(table_counters.full_rebuilds);
}

void
ServerNG::get_owner_tag_(mdsproto::Methods::GetOwnerTagParams::Reader& reader,
                         mdsproto::Methods::GetOwnerTagResults::Builder& builder)
{
    const std::string nspace(reader.getNspace().begin(),
                             reader.getNspace().size());

    const vd::OwnerTag owner_tag = db_->open(nspace)->owner_tag();
    builder.setOwnerTag(static_cast<const uint64_t>(owner_tag));
}

}
