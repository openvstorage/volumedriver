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

#include "ClientNG.h"
#include "Utils.h"

#include "../MDSNodeConfig.h"

#include <boost/array.hpp>

#include <capnp/message.h>
#include <capnp/serialize.h>

#include <youtils/Assert.h>
#include <youtils/SourceOfUncertainty.h>

namespace metadata_server
{

namespace ba = boost::asio;
namespace yt = youtils;
namespace mdsproto = metadata_server_protocol;
namespace vd = volumedriver;

struct TableHandle
    : public TableInterface
{
    friend class ClientNG;

    DECLARE_LOGGER("MetaDataServerTableHandle");

    TableHandle(ClientNG::Ptr client,
                const std::string& nspace)
        : nspace_(nspace)
        , client_(client)
    {}

    virtual ~TableHandle() = default;

    virtual void
    multiset(const Records& records,
             Barrier barrier,
             vd::OwnerTag owner_tag) override final
    {
        auto b([&](mdsproto::Methods::MultiSetParams::Builder& builder)
               {
                   // LOG_TRACE(nspace_ << ": building MultiSet of size " << records.size());

                   builder.setNspace(nspace_);
                   builder.setBarrier(barrier == Barrier::T);
                   builder.setOwnerTag(static_cast<uint64_t>(owner_tag));

                   size_t idx = 0;
                   auto l = builder.initRecords(records.size());

                   for (const auto& r : records)
                   {
                       auto e = l[idx];
                       e.setKey(capnp::Data::Reader(static_cast<const kj::byte*>(r.key.data),
                                                    r.key.size));
                       e.setVal(capnp::Data::Reader(static_cast<const kj::byte*>(r.val.data),
                                                    r.val.size));
                       ++idx;
                   }
               });

        auto r([&](mdsproto::Methods::MultiSetResults::Reader&)
               {
                   // LOG_TRACE(nspace_ << ": reading MultiSet results");
               });

        client_->interact_<mdsproto::RequestHeader::Type::MultiSet>(std::move(b),
                                                                    std::move(r));
    }

    virtual MaybeStrings
    multiget(const Keys& keys) override final
    {
        auto b([&](mdsproto::Methods::MultiGetParams::Builder& builder)
               {
                   // LOG_TRACE(nspace_ << ": building MultiGet of size " << keys.size());

                   builder.setNspace(nspace_);

                   size_t idx = 0;
                   auto l = builder.initKeys(keys.size());

                   for (const auto& k : keys)
                   {
                       l.set(idx++,
                             capnp::Data::Reader(static_cast<const kj::byte*>(k.data),
                                                 k.size));
                   }
               });

        MaybeStrings res;

        auto r([&](mdsproto::Methods::MultiGetResults::Reader& reader)
               {
                   // LOG_TRACE(nspace_ << ": reading MultiGet results");

                   res.reserve(keys.size());
                   for (const auto& v : reader.getValues())
                   {
                       if (v.size() == 0)
                       {
                           res.emplace_back(boost::none);
                       }
                       else
                       {
                           res.emplace_back(std::string(reinterpret_cast<const char*>(v.begin()),
                                                        v.size()));
                       }
                   }
               });

        client_->interact_<mdsproto::RequestHeader::Type::MultiGet>(std::move(b),
                                                                    std::move(r));

        return res;
    }

    virtual void
    apply_relocations(const vd::ScrubId& scrub_id,
                      const vd::SCOCloneID cid,
                      const TableInterface::RelocationLogs& relocs) override final
    {
        auto b([&](mdsproto::Methods::ApplyRelocationLogsParams::Builder& builder)
               {
                   // LOG_TRACE(nspace_ << ": building ApplyRelocationLogs of size " <<
                   //           relocs.size() << ", scrub_id " << scrub_id);

                   builder.setNspace(nspace_);
                   builder.setScrubId(static_cast<const yt::UUID&>(scrub_id).str());
                   builder.setCloneId(cid);

                   size_t idx = 0;
                   auto l = builder.initLogs(relocs.size());

                   for (const auto& r : relocs)
                   {
                       l.set(idx++,
                             capnp::Text::Reader(r));
                   }
               });

        auto r([&](mdsproto::Methods::ApplyRelocationLogsResults::Reader&)
               {
                   // LOG_TRACE(nspace_ << ": reading ApplyRelocationLogs results");
               });

        client_->interact_<mdsproto::RequestHeader::Type::ApplyRelocationLogs>(std::move(b),
                                                                               std::move(r));
    }

    virtual void
    clear(vd::OwnerTag owner_tag) override final
    {
        auto b([&](mdsproto::Methods::ClearParams::Builder& builder)
               {
                   // LOG_TRACE(nspace_ << ": building Clear request");
                   builder.setNspace(nspace_);
                   builder.setOwnerTag(static_cast<uint64_t>(owner_tag));
               });

        auto r([&](mdsproto::Methods::ClearResults::Reader&)
               {
                   // LOG_TRACE(nspace_ << ": reading Clear response");
               });

        client_->interact_<mdsproto::RequestHeader::Type::Clear>(std::move(b),
                                                                 std::move(r));
    }

    virtual Role
    get_role() const override final
    {
        auto b([&](mdsproto::Methods::GetRoleParams::Builder& builder)
               {
                   // LOG_TRACE(nspace_ << ": building GetRole request");
                   builder.setNspace(nspace_);
               });

        Role role = Role::Slave;

        auto r([&](mdsproto::Methods::GetRoleResults::Reader& reader)
               {
                   // LOG_TRACE(nspace_ << ": reading GetRole response");
                   role = translate_role(reader.getRole());
               });

        client_->interact_<mdsproto::RequestHeader::Type::GetRole>(std::move(b),
                                                                   std::move(r));

        return role;
    }

    virtual vd::OwnerTag
    owner_tag() const override final
    {
        auto b([&](mdsproto::Methods::GetOwnerTagParams::Builder& builder)
               {
                   // LOG_TRACE(nspace_ << ": building GetOwnerTag request");
                   builder.setNspace(nspace_);
               });

        vd::OwnerTag owner_tag(0);

        auto r([&](mdsproto::Methods::GetOwnerTagResults::Reader& reader)
               {
                   // LOG_TRACE(nspace_ << ": reading GetOwnerTag response");
                   owner_tag = vd::OwnerTag(reader.getOwnerTag());
               });

        client_->interact_<mdsproto::RequestHeader::Type::GetOwnerTag>(std::move(b),
                                                                       std::move(r));

        return owner_tag;
    }

    virtual void
    set_role(Role role,
             vd::OwnerTag owner_tag) override final
    {
        auto b([&](mdsproto::Methods::SetRoleParams::Builder& builder)
               {
                   // LOG_TRACE(nspace_ << ": building SetRole request");
                   builder.setNspace(nspace_);
                   builder.setRole(translate_role(role));
                   builder.setOwnerTag(static_cast<uint64_t>(owner_tag));
               });

        auto r([&](mdsproto::Methods::SetRoleResults::Reader&)
               {
                   // LOG_TRACE(nspace_ << ": reading SetRole response");
               });

        client_->interact_<mdsproto::RequestHeader::Type::SetRole>(std::move(b),
                                                                   std::move(r));
    }

    virtual size_t
    catch_up(vd::DryRun dry_run) override final
    {
        auto b([&](mdsproto::Methods::CatchUpParams::Builder& builder)
               {
                   // LOG_TRACE(nspace_ << ": building CatchUp request");
                   builder.setNspace(nspace_);
                   builder.setDryRun(dry_run == vd::DryRun::T);
               });

        size_t num_tlogs = 0;

        auto r([&](mdsproto::Methods::CatchUpResults::Reader& reader)
               {
                   // LOG_TRACE(nspace_ << ": reading CatchUp response");
                   num_tlogs = reader.getNumTLogs();
               });

        client_->interact_<mdsproto::RequestHeader::Type::CatchUp>(std::move(b),
                                                                   std::move(r));

        return num_tlogs;
    }

    virtual const std::string&
    nspace() const override final
    {
        return nspace_;
    }

    virtual TableCounters
    get_counters(vd::Reset reset) override final
    {
        auto b([&](mdsproto::Methods::GetTableCountersParams::Builder& builder)
               {
                   // LOG_TRACE(nspace_ << ": building CatchUp request");
                   builder.setNspace(nspace_);
                   builder.setReset(reset == vd::Reset::T);
               });

        TableCounters counters;

        auto r([&](mdsproto::Methods::GetTableCountersResults::Reader& reader)
               {
                   const auto c = reader.getCounters();
                   counters.total_tlogs_read = c.getTotalTLogsRead();
                   counters.incremental_updates = c.getIncrementalUpdates();
                   counters.full_rebuilds = c.getFullRebuilds();
               });

        client_->interact_<mdsproto::RequestHeader::Type::GetTableCounters>(std::move(b),
                                                                            std::move(r));
        return counters;
    }

    const std::string nspace_;
    ClientNG::Ptr client_;
};

ClientNG::Ptr
ClientNG::create(const vd::MDSNodeConfig& cfg,
                 size_t shmem_size,
                 const boost::optional<std::chrono::seconds>& timeout,
                 ForceRemote force_remote)
{
    return Ptr(new ClientNG(cfg,
                            shmem_size,
                            timeout,
                            force_remote));
}

ClientNG::ClientNG(const vd::MDSNodeConfig& cfg,
                   size_t shmem_size,
                   const boost::optional<std::chrono::seconds>& timeout,
                   ForceRemote force_remote)
    : client_(cfg.address(),
              cfg.port(),
              timeout,
              force_remote)
    , mr_(shmem_size ? new yt::SharedMemoryRegion(shmem_size) : nullptr)
    , timeout_(timeout)
{
    LOG_INFO(this << ": " << cfg << ", shmem size " << shmem_size <<
             ", is local: " << client_.is_local() << ", timeout: " <<
             (timeout_ ? boost::lexical_cast<std::string>(timeout->count()) : "--") <<
             " secs");
}

ClientNG::~ClientNG()
{
    LOG_INFO(this << ": terminating");
}

TableInterfacePtr
ClientNG::open(const std::string& nspace)
{
    auto b([&](mdsproto::Methods::OpenParams::Builder& builder)
           {
               // LOG_TRACE(nspace << ": building Open request");
               builder.setNspace(nspace);
           });

    auto r([&](mdsproto::Methods::OpenResults::Reader&)
           {
               // LOG_TRACE(nspace << ": reading Open response");
           });

    interact_<mdsproto::RequestHeader::Type::Open>(std::move(b),
                                                   std::move(r));

    return std::make_shared<TableHandle>(shared_from_this(),
                                         nspace);
}

void
ClientNG::drop(const std::string& nspace)
{
    auto b([&](mdsproto::Methods::DropParams::Builder& builder)
           {
               // LOG_TRACE(nspace << ": building Drop request");
               builder.setNspace(nspace);
           });

    auto r([&](mdsproto::Methods::DropResults::Reader&)
           {
               // LOG_TRACE(nspace << ": reading Drop response");
           });

    interact_<mdsproto::RequestHeader::Type::Drop>(std::move(b),
                                                   std::move(r));
}

std::vector<std::string>
ClientNG::list_namespaces()
{
    auto b([&](mdsproto::Methods::ListParams::Builder&)
           {
               // LOG_TRACE("building List request");
           });

    std::vector<std::string> res;

    auto r([&](mdsproto::Methods::ListResults::Reader& reader)
           {
               // LOG_TRACE("reading List response");

               const auto& nspaces = reader.getNspaces();
               res.reserve(nspaces.size());

               for (const auto& n : nspaces)
               {
                   res.emplace_back(std::string(n.begin(),
                                                n.size()));
               }
           });

    interact_<mdsproto::RequestHeader::Type::List>(std::move(b),
                                                   std::move(r));

    return res;
}

void
ClientNG::ping(const std::vector<uint8_t>& out,
               std::vector<uint8_t>& in)
{
    auto b([&](mdsproto::Methods::PingParams::Builder& builder)
           {
               capnp::Data::Reader r(out.data(),
                                     out.size());
               builder.setData(r);
           });

    auto r([&](mdsproto::Methods::PingResults::Reader& reader)
           {
               auto data(reader.getData());
               THROW_UNLESS(data.size() <= in.size());
               memcpy(in.data(),
                      data.begin(),
                      data.size());
           });

    interact_<mdsproto::RequestHeader::Type::Ping>(std::move(b),
                                                   std::move(r));
}

void
ClientNG::prepare_shmem_()
{
    // This is a serious performance hog, but Cap'n Proto shows all sorts of weird errors
    // (exceptions about missing \0-terminators of strings) when not doing it. Sigh.
    // Dear Cap'n, while it might appear to be a good idea to force your users to memset
    // the buffer to prevent information leaks in hostile environments this also punishes
    // those in a controlled environment, FFS!
#if 1
    if (mr_ != nullptr)
    {
        memset(mr_->address(),
               0x0,
               mr_->size());
    }
#endif
}

template<enum metadata_server_protocol::RequestHeader::Type T,
         typename Build>
size_t
ClientNG::send_shmem_(mdsproto::Tag tag,
                      Build&& build)
{
    using Traits = mdsproto::RequestTraits<T>;

    prepare_shmem_();

    capnp::FlatMessageBuilder builder(kj::arrayPtr(static_cast<capnp::word*>(mr_->address()),
                                                   mr_->size() / sizeof(capnp::word)));

    auto root(builder.initRoot<typename Traits::Params>());

    build(root);

    const size_t size = capnp::computeSerializedSizeInWords(builder) *
        sizeof(capnp::word);

    const mdsproto::RequestHeader hdr(Traits::request_type,
                                      size,
                                      tag,
                                      mr_->id(),
                                      0,
                                      mr_->id(),
                                      size);

    // LOG_TRACE("sending " << hdr.request_type <<
    //           ", tag " << hdr.tag <<
    //           ", size " << hdr.size <<
    //           ", out region " << hdr.out_region << ", off " << hdr.out_offset <<
    //           ", in region " << hdr.in_region << ", off " << hdr.in_offset);

    client_.send(ba::buffer(&hdr,
                            sizeof(hdr)),
                 timeout_);

    ++out_counters_.messages;
    out_counters_.data_bytes += size;
    out_counters_.data_bytes_sqsum += size * size;

    return size;
}

template<enum metadata_server_protocol::RequestHeader::Type T,
         typename Build>
size_t
ClientNG::send_inband_(mdsproto::Tag tag,
                       Build&& build)
{
    using Traits = mdsproto::RequestTraits<T>;

    capnp::MallocMessageBuilder builder;
    auto root(builder.initRoot<typename Traits::Params>());

    build(root);

    kj::Array<capnp::word> data(capnp::messageToFlatArray(builder));

    prepare_shmem_();

    const mdsproto::RequestHeader hdr(Traits::request_type,
                                      data.size() * sizeof(capnp::word),
                                      tag,
                                      yt::SharedMemoryRegionId(0),
                                      0,
                                      use_shmem_() ?
                                      mr_->id() :
                                      yt::SharedMemoryRegionId(0),
                                      0);

    boost::array<ba::const_buffer, 2> bufs = {{
            ba::buffer(&hdr,
                       sizeof(hdr)),
            ba::buffer(data.begin(),
                       data.size() * sizeof(capnp::word))
        }};

    // LOG_TRACE("sending " << hdr.request_type <<
    //           ", tag " << hdr.tag <<
    //           ", size " << hdr.size <<
    //           ", out region " << hdr.out_region << ", off " << hdr.out_offset <<
    //           ", in region " << hdr.in_region << ", off " << hdr.in_offset);

    client_.send(bufs,
                 timeout_);

    ++out_counters_.messages;
    out_counters_.data_bytes += hdr.size;
    out_counters_.data_bytes_sqsum += hdr.size * hdr.size;

    return hdr.size;
}

template<enum mdsproto::RequestHeader::Type T,
         typename Build,
         typename Read>
void
ClientNG::interact_(Build&& build,
                    Read&& read)
{
    boost::lock_guard<decltype(lock_)> g(lock_);

    const mdsproto::Tag txtag(reinterpret_cast<uint64_t>(this));

    size_t txoff = 0;

    bool use_shmem = use_shmem_();
    if (use_shmem)
    {
        try
        {
            txoff = send_shmem_<T>(txtag,
                                    std::move(build));
        }
        catch (kj::Exception& e)
        {
            LOG_ERROR("Failed to build shmem message " << e.getDescription().cStr() <<
                      " - falling back to socket");
            ++out_counters_.shmem_overruns;
            use_shmem = false;
        }
    }

    if (not use_shmem)
    {
        send_inband_<T>(txtag,
                        std::move(build));
    }

    recv_<T>(txtag,
             txoff,
             std::move(read));
}

template<enum mdsproto::RequestHeader::Type T,
         typename Read>
void
ClientNG::recv_(mdsproto::Tag txtag,
                size_t txoff,
                Read&& read)
{
    mdsproto::ResponseHeader rxhdr;

    client_.recv(ba::buffer(&rxhdr,
                            sizeof(rxhdr)),
                 timeout_);

    if (rxhdr.magic != mdsproto::magic)
    {
        LOG_ERROR("Response lacks our protocol magic, giving up");
        throw mdsproto::NoMagicException("no magic key in received header");
    }

    // LOG_TRACE("received hdr " << rxhdr.response_type <<
    //           ", tag " << rxhdr.tag <<
    //           ", size " << rxhdr.size <<
    //           ", flags " << rxhdr.flags);

    TODO("AR: better exceptions");
    THROW_WHEN(rxhdr.tag != txtag);

    ++in_counters_.messages;
    in_counters_.data_bytes += rxhdr.size;
    in_counters_.data_bytes_sqsum += rxhdr.size * rxhdr.size;

    if (rxhdr.size)
    {
        if ((rxhdr.flags bitand mdsproto::ResponseHeader::Flags::UseShmem) == 0)
        {
            if (use_shmem_())
            {
                ++in_counters_.shmem_overruns;
            }

            std::vector<capnp::word> rxbuf(rxhdr.size / sizeof(capnp::word));

            client_.recv(ba::buffer(rxbuf),
                         timeout_);

            capnp::FlatArrayMessageReader reader(kj::arrayPtr(rxbuf.data(),
                                                              rxbuf.size()));
            handle_response_<T>(rxhdr,
                                reader,
                                std::move(read));
        }
        else
        {
            THROW_UNLESS(use_shmem_());

            const uint8_t* addr = static_cast<const uint8_t*>(mr_->address()) + txoff;

            THROW_UNLESS(addr + rxhdr.size <=
                         static_cast<const uint8_t*>(mr_->address()) + mr_->size());

            auto seg(kj::arrayPtr(reinterpret_cast<const capnp::word*>(addr),
                                  rxhdr.size / sizeof(capnp::word)));

            capnp::SegmentArrayMessageReader reader(kj::arrayPtr(&seg, 1));

            handle_response_<T>(rxhdr,
                                reader,
                                std::move(read));
        }
    }
}

template<enum mdsproto::RequestHeader::Type T,
         typename Read>
void
ClientNG::handle_response_(const mdsproto::ResponseHeader& hdr,
                           capnp::MessageReader& reader,
                           Read&& read)
{
    using Traits = mdsproto::RequestTraits<T>;

    if (hdr.response_type != mdsproto::ResponseHeader::Type::Ok)
    {
        auto root(reader.getRoot<mdsproto::Error>());
        const std::string msg(root.getMessage().begin(),
                              root.getMessage().size());
        LOG_ERROR("Got an error: " << msg);
        switch (root.getErrorType()) {
        case mdsproto::ErrorType::OWNER_TAG_MISMATCH:
            throw vd::OwnerTagMismatchException(msg.c_str());
            break;
        default:
            throw fungi::IOException(msg.c_str());
        }
    }
    else
    {
        auto root(reader.getRoot<typename Traits::Results>());
        read(root);
    }
}

}
