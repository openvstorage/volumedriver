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

#include <youtils/Assert.h>

namespace vfsprotocol
{

const char*
request_type_to_string(const RequestType type)
{
    // Attempt to have our cake and eat it too:
    // the first switch statement is there to have the compiler emit a warning
    // if we forgot a case, while the second one will deal with unknown opcodes.
    // ** So if you do see the warning, make sure to update the second switch
    // statement too! **
    switch (type)
    {
    case RequestType::Read:
    case RequestType::Write:
    case RequestType::Sync:
    case RequestType::GetSize:
    case RequestType::Resize:
    case RequestType::Delete:
    case RequestType::Transfer:
    case RequestType::Ping:
        break;
    }

    switch (type)
    {
    case RequestType::Read:
        return "Read";
    case RequestType::Write:
        return "Write";
    case RequestType::Sync:
        return "Sync";
    case RequestType::GetSize:
        return "GetSize";
    case RequestType::Resize:
        return "Resize";
    case RequestType::Delete:
        return "Delete";
    case RequestType::Transfer:
        return "Transfer";
    case RequestType::Ping:
        return "Ping";
    default:
        return "Unknown";
    }

    UNREACHABLE;
}

const char*
response_type_to_string(ResponseType type)
{
    // Attempt to have our cake and eat it too:
    // the first switch statement is there to have the compiler emit a warning
    // if we forgot a case, while the second one will deal with unknown opcodes.
    // ** So if you do see the warning, make sure to update the second switch
    // statement too! **
    switch (type)
    {
    case ResponseType::Ok:
    case ResponseType::UnknownRequest:
    case ResponseType::ProtocolError:
    case ResponseType::ObjectNotRunningHere:
    case ResponseType::IOError:
    case ResponseType::Timeout:
        break;
    }

    switch (type)
    {
    case ResponseType::Ok:
        return "Ok";
    case ResponseType::UnknownRequest:
        return "UnknownRequest";
    case ResponseType::ProtocolError:
        return "ProtocolError";
    case ResponseType::ObjectNotRunningHere:
        return "ObjectNotRunningHere";
    case ResponseType::IOError:
        return "IOError";
    case ResponseType::Timeout:
        return "Timeout";
    default:
        return "Unknown";
    }

    UNREACHABLE;
}


}
