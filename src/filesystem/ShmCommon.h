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

#ifndef __SHM_COMMON_H
#define __SHM_COMMON_H

#include <string>

struct ShmSegmentDetails
{
    std::string cluster_id;
    std::string vrouter_id;

    const std::string
    id() const
    {
        using namespace std::literals::string_literals;
        return cluster_id + "-"s + vrouter_id;
    }

    const std::string
    control_endpoint() const
    {
        using namespace std::literals::string_literals;
        // TODO: make the location configurable
        return "/tmp/ovs-shm-ctl-"s + id() + ".socket"s;
    }

    ShmSegmentDetails(const std::string& cid,
                      const std::string& vid)
        : cluster_id(cid)
        , vrouter_id(vid)
    {}

    ~ShmSegmentDetails() = default;
};

#endif // __SHM_COMMON_H
