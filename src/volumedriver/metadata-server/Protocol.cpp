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

#include "Protocol.h"

#include <iostream>
#include <map>

#include <youtils/StreamUtils.h>

namespace metadata_server_protocol
{

namespace yt = youtils;

namespace
{

void
req_hdr_reminder(RequestHeader::Type t) __attribute__((unused));

void
req_hdr_reminder(RequestHeader::Type t)
{
#define C(req)                                  \
    case RequestHeader::Type::req:              \
        break

    switch (t)
    {
        C(Drop);
        C(Clear);
        C(List);
        C(MultiGet);
        C(MultiSet);
        C(SetRole);
        C(GetRole);
        C(Open);
        C(Ping);
        C(ApplyRelocationLogs);
        C(CatchUp);
        C(GetTableCounters);
        // If the compiler yells at you that you've forgotten dealing with an enum
        // value chances are that it's also missing from the translations map below.
        // If so add it RIGHT NOW.
    }

#undef C
}

void
rsp_hdr_reminder(ResponseHeader::Type t) __attribute__((unused));

void
rsp_hdr_reminder(ResponseHeader::Type t)
{
#define C(rsp)                                  \
    case ResponseHeader::Type::rsp:             \
        break

    switch (t)
    {
        C(Ok);
        C(UnknownRequest);
        C(ProtocolError);
        C(Error);
        // If the compiler yells at you that you've forgotten dealing with an enum
        // value chances are that it's also missing from the translations map below.
        // If so add it RIGHT NOW.
    }

#undef C
}

}

std::ostream&
operator<<(std::ostream& os,
           RequestHeader::Type t)
{
#define P(name)                                 \
    { RequestHeader::Type::name, #name }

    static const std::map<RequestHeader::Type,
                          std::string> translations = {
        P(Drop),
        P(Clear),
        P(List),
        P(MultiGet),
        P(MultiSet),
        P(SetRole),
        P(GetRole),
        P(Open),
        P(Ping),
        P(ApplyRelocationLogs),
        P(CatchUp),
        P(GetTableCounters),
    };

#undef P

    return yt::StreamUtils::stream_out(translations,
                                       os,
                                       t);
}

std::ostream&
operator<<(std::ostream& os,
           ResponseHeader::Type t)
{
#define P(name)                                 \
    { ResponseHeader::Type::name, #name }

    static const std::map<ResponseHeader::Type,
                          std::string> translations = {
        P(Ok),
        P(UnknownRequest),
        P(ProtocolError),
        P(Error)
    };

#undef P

    return yt::StreamUtils::stream_out(translations,
                                       os,
                                       t);
}

}
