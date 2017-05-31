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

#ifndef NETWORK_XIO_REQUEST_FWD_H_
#define NETWORK_XIO_REQUEST_FWD_H_

#include <boost/intrusive_ptr.hpp>

namespace volumedriverfs
{

struct NetworkXioRequest;

// intrusive_ptr to manage the lifetimes of NetworkXioRequest objects
// as shared_ptr doesn't mix well with intrusive containers or plain C
// interfaces (don't forget to bump the ref count manually (e.g.
// intrusive_add_ref) when doing so).
using NetworkXioRequestPtr = boost::intrusive_ptr<NetworkXioRequest>;

}

#endif // !NETWORK_XIO_REQUEST_FWD_H_
