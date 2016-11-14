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

#ifndef NETWORK_XIO_WORK_H_
#define NETWORK_XIO_WORK_H_

#include <functional>
#include <libxio.h>

namespace volumedriverfs
{

struct Work;

typedef std::function<void(Work*)> workitem_func_t;

struct Work
{
    workitem_func_t func;
    workitem_func_t func_ctrl;
    workitem_func_t dispatch_ctrl_request;
    bool is_ctrl;
};

} //namespace

#endif //NETWORK_XIO_WORK_H_
