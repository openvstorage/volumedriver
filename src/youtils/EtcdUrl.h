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

#ifndef YT_ETCD_URL_H_
#define YT_ETCD_URL_H_

#include "Url.h"

namespace youtils
{

struct EtcdUrlTraits
{
    static boost::optional<std::string>
    scheme()
    {
        static const std::string s("etcd");
        return s;
    }

    static boost::optional<uint16_t>
    default_port()
    {
        return 2379;
    }
};

using EtcdUrl = Url<EtcdUrlTraits>;

}

#endif // YT_ETCD_URL_H_
