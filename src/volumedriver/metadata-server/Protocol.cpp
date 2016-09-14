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
        C(GetOwnerTag);
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
        P(GetOwnerTag),
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
