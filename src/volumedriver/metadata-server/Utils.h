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

#ifndef META_DATA_SERVER_UTILS_H_
#define META_DATA_SERVER_UTILS_H_

#include "Interface.h"
#include "Protocol.h"

#include <iosfwd>

namespace metadata_server
{

std::ostream&
operator<<(std::ostream& os,
           Role r);

std::istream&
operator>>(std::istream& is,
           Role& r);

metadata_server_protocol::Role
translate_role(Role role);

Role
translate_role(metadata_server_protocol::Role role);

}

#endif // !META_DATA_SERVER_UTILS_H_
