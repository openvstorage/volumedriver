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
    ServerNG(DataBaseInterfacePtr db,
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

    template<typename C>
    void
    recv_header_(C& conn,
                 ConnectionStatePtr state);

    template<typename C>
    void
    get_data_(C& conn,
              ConnectionStatePtr state,
              std::shared_ptr<metadata_server_protocol::RequestHeader> hdr);

    template<typename C>
    void
    recv_data_(C& conn,
               ConnectionStatePtr state,
               std::shared_ptr<metadata_server_protocol::RequestHeader> hdr);

    template<typename C>
    void
    dispatch_(C& conn,
              ConnectionStatePtr state,
              const metadata_server_protocol::RequestHeader& hdr,
              capnp::MessageReader& reader);

    template<typename C>
    void
    send_response_(C& conn,
                   ConnectionStatePtr state,
                   metadata_server_protocol::ResponseHeader::Type rsp,
                   metadata_server_protocol::Tag tag,
                   capnp::MessageBuilder& builder);

    template<typename C>
    void
    send_response_inband_(C& conn,
                          ConnectionStatePtr state,
                          metadata_server_protocol::ResponseHeader::Type rsp,
                          metadata_server_protocol::Tag tag,
                          capnp::MessageBuilder& builder);

    template<typename C>
    void
    send_response_shmem_(C& conn,
                         ConnectionStatePtr state,
                         metadata_server_protocol::ResponseHeader::Type rsp,
                         metadata_server_protocol::Tag tag,
                         capnp::FlatMessageBuilder& builder);

    template<typename C>
    void
    error_(C& conn,
           ConnectionStatePtr state,
           const metadata_server_protocol::ResponseHeader::Type rsp,
           const metadata_server_protocol::Tag tag,
           const std::string& msg);

    template<enum metadata_server_protocol::RequestHeader::Type r,
             typename C,
             typename Traits = metadata_server_protocol::RequestTraits<r>>
    void
    handle_(C& conn,
            ConnectionStatePtr state,
            const metadata_server_protocol::RequestHeader& hdr,
            capnp::MessageReader& reader,
            void (ServerNG::*mem_fn)(typename Traits::Params::Reader&,
                                     typename Traits::Results::Builder&));

    template<enum metadata_server_protocol::RequestHeader::Type r,
             typename C,
             typename Traits = metadata_server_protocol::RequestTraits<r>>
    void
    handle_shmem_(C& conn,
                  ConnectionStatePtr state,
                  const metadata_server_protocol::RequestHeader& hdr,
                  capnp::MessageReader& reader,
                  void (ServerNG::*mem_fn)(typename Traits::Params::Reader&,
                                           typename Traits::Results::Builder&));

    template<enum metadata_server_protocol::RequestHeader::Type r,
             typename C,
             typename Traits = metadata_server_protocol::RequestTraits<r>>
    void
    do_handle_(C& conn,
               ConnectionStatePtr state,
               const metadata_server_protocol::RequestHeader& hdr,
               capnp::MessageBuilder& builder,
               capnp::MessageReader& reader,
               void (ServerNG::*mem_fn)(typename Traits::Params::Reader&,
                                        typename Traits::Results::Builder&));

    void
    open_(metadata_server_protocol::Methods::OpenParams::Reader& reader,
          metadata_server_protocol::Methods::OpenResults::Builder& builder);

    void
    drop_(metadata_server_protocol::Methods::DropParams::Reader& reader,
          metadata_server_protocol::Methods::DropResults::Builder& builder);

    void
    clear_(metadata_server_protocol::Methods::ClearParams::Reader& reader,
           metadata_server_protocol::Methods::ClearResults::Builder& builder);

    void
    multiget_(metadata_server_protocol::Methods::MultiGetParams::Reader& reader,
              metadata_server_protocol::Methods::MultiGetResults::Builder& builder);

    void
    multiset_(metadata_server_protocol::Methods::MultiSetParams::Reader& reader,
              metadata_server_protocol::Methods::MultiSetResults::Builder& builder);

    void
    set_role_(metadata_server_protocol::Methods::SetRoleParams::Reader& reader,
              metadata_server_protocol::Methods::SetRoleResults::Builder& builder);

    void
    get_role_(metadata_server_protocol::Methods::GetRoleParams::Reader& reader,
              metadata_server_protocol::Methods::GetRoleResults::Builder& builder);

    void
    list_namespaces_(metadata_server_protocol::Methods::ListParams::Reader& reader,
                     metadata_server_protocol::Methods::ListResults::Builder& builder);

    void
    ping_(metadata_server_protocol::Methods::PingParams::Reader& reader,
          metadata_server_protocol::Methods::PingResults::Builder& builder);

    void
    apply_relocation_logs_(metadata_server_protocol::Methods::ApplyRelocationLogsParams::Reader& reader,
                           metadata_server_protocol::Methods::ApplyRelocationLogsResults::Builder& builder);

    void
    catch_up_(metadata_server_protocol::Methods::CatchUpParams::Reader& reader,
              metadata_server_protocol::Methods::CatchUpResults::Builder& builder);

};

}

#endif // !META_DATA_SERVER_SERVER_H_
