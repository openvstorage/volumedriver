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
