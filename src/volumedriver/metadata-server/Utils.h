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
