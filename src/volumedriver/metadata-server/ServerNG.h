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

#ifndef META_DATA_SERVER_SERVER_H_
#define META_DATA_SERVER_SERVER_H_

#include "Interface.h"
#include "Protocol.h"

#include <unordered_set>

#include <boost/thread.hpp>

#include <capnp/message.h>

#include <youtils/LocORemServer.h>
#include <youtils/Logging.h>

namespace metadata_server
{

class ServerNG
{
public:
    ServerNG(DataBaseInterfacePtr,
             const std::string& addr,
             const uint16_t port,
             const boost::optional<std::chrono::seconds>& timeout = boost::none,
             const uint32_t nthreads = boost::thread::hardware_concurrency());

    ~ServerNG() = default;

    ServerNG(const ServerNG&) = delete;

    ServerNG&
    operator=(const ServerNG&) = delete;

    DataBaseInterfacePtr
    database()
    {
        return db_;
    }

private:
    DECLARE_LOGGER("MetaDataServerNG");

    struct ConnectionState;
    using ConnectionStatePtr = std::shared_ptr<ConnectionState>;

    const boost::optional<std::chrono::seconds> timeout_;
    DataBaseInterfacePtr db_;
    youtils::LocORemServer server_;

    template<typename Connection>
    void
    recv_header_(Connection&,
                 ConnectionStatePtr);

    template<typename Connection>
    void
    get_data_(Connection&,
              ConnectionStatePtr,
              std::shared_ptr<metadata_server_protocol::RequestHeader>);

    template<typename Connection>
    void
    recv_data_(Connection&,
               ConnectionStatePtr,
               std::shared_ptr<metadata_server_protocol::RequestHeader>);

    template<typename Connection>
    void
    dispatch_(Connection&,
              ConnectionStatePtr,
              const metadata_server_protocol::RequestHeader&,
              capnp::MessageReader&);

    template<typename Connection>
    void
    send_response_(Connection&,
                   ConnectionStatePtr,
                   metadata_server_protocol::ResponseHeader::Type,
                   metadata_server_protocol::Tag,
                   capnp::MessageBuilder&);

    template<typename Connection>
    void
    send_response_inband_(Connection&,
                          ConnectionStatePtr,
                          metadata_server_protocol::ResponseHeader::Type,
                          metadata_server_protocol::Tag,
                          capnp::MessageBuilder&);

    template<typename Connection>
    void
    send_response_shmem_(Connection&,
                         ConnectionStatePtr,
                         metadata_server_protocol::ResponseHeader::Type,
                         metadata_server_protocol::Tag,
                         capnp::FlatMessageBuilder&);

    template<typename Connection>
    void
    error_(Connection&,
           ConnectionStatePtr,
           const metadata_server_protocol::ResponseHeader::Type,
           const metadata_server_protocol::Tag,
           const std::string&);

    template<enum metadata_server_protocol::RequestHeader::Type r,
             typename Connection,
             typename Traits = metadata_server_protocol::RequestTraits<r>>
    void
    handle_(Connection&,
            ConnectionStatePtr,
            const metadata_server_protocol::RequestHeader&,
            capnp::MessageReader&,
            void (ServerNG::*mem_fn)(typename Traits::Params::Reader&,
                                     typename Traits::Results::Builder&));

    template<enum metadata_server_protocol::RequestHeader::Type r,
             typename Connection,
             typename Traits = metadata_server_protocol::RequestTraits<r>>
    void
    handle_shmem_(Connection&,
                  ConnectionStatePtr,
                  const metadata_server_protocol::RequestHeader&,
                  capnp::MessageReader&,
                  void (ServerNG::*mem_fn)(typename Traits::Params::Reader&,
                                           typename Traits::Results::Builder&));

    template<enum metadata_server_protocol::RequestHeader::Type r,
             typename Connection,
             typename Traits = metadata_server_protocol::RequestTraits<r>>
    void
    do_handle_(Connection&,
               ConnectionStatePtr,
               const metadata_server_protocol::RequestHeader&,
               capnp::MessageBuilder&,
               capnp::MessageReader&,
               void (ServerNG::*mem_fn)(typename Traits::Params::Reader&,
                                        typename Traits::Results::Builder&));

    void
    open_(metadata_server_protocol::Methods::OpenParams::Reader&,
          metadata_server_protocol::Methods::OpenResults::Builder&);

    void
    drop_(metadata_server_protocol::Methods::DropParams::Reader&,
          metadata_server_protocol::Methods::DropResults::Builder&);

    void
    clear_(metadata_server_protocol::Methods::ClearParams::Reader&,
           metadata_server_protocol::Methods::ClearResults::Builder&);

    void
    multiget_(metadata_server_protocol::Methods::MultiGetParams::Reader&,
              metadata_server_protocol::Methods::MultiGetResults::Builder&);

    void
    multiset_(metadata_server_protocol::Methods::MultiSetParams::Reader&,
              metadata_server_protocol::Methods::MultiSetResults::Builder&);

    void
    set_role_(metadata_server_protocol::Methods::SetRoleParams::Reader&,
              metadata_server_protocol::Methods::SetRoleResults::Builder&);

    void
    get_role_(metadata_server_protocol::Methods::GetRoleParams::Reader&,
              metadata_server_protocol::Methods::GetRoleResults::Builder&);

    void
    list_namespaces_(metadata_server_protocol::Methods::ListParams::Reader&,
                     metadata_server_protocol::Methods::ListResults::Builder&);

    void
    ping_(metadata_server_protocol::Methods::PingParams::Reader&,
          metadata_server_protocol::Methods::PingResults::Builder&);

    void
    apply_relocation_logs_(metadata_server_protocol::Methods::ApplyRelocationLogsParams::Reader&,
                           metadata_server_protocol::Methods::ApplyRelocationLogsResults::Builder&);

    void
    catch_up_(metadata_server_protocol::Methods::CatchUpParams::Reader&,
              metadata_server_protocol::Methods::CatchUpResults::Builder&);

    void
    get_table_counters_(metadata_server_protocol::Methods::GetTableCountersParams::Reader&,
                        metadata_server_protocol::Methods::GetTableCountersResults::Builder&);

    void
    get_owner_tag_(metadata_server_protocol::Methods::GetOwnerTagParams::Reader&,
                   metadata_server_protocol::Methods::GetOwnerTagResults::Builder&);
};

}

#endif // !META_DATA_SERVER_SERVER_H_
