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

#ifndef REQUEST_DISPATCHER_CALLBACK_H_
#define REQUEST_DISPATCHER_CALLBACK_H_

struct ovs_aio_request;

namespace libovsvolumedriver
{

struct RequestDispatcherCallback
{
    virtual ~RequestDispatcherCallback() = default;

    virtual void
    complete_request(ovs_aio_request&,
                     ssize_t ret,
                     int err,
                     bool schedule) = 0;
};

}

#endif // !REQUEST_DISPATCHER_CALLBACK_H_
