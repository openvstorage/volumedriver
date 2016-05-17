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

#ifndef CLUSTER_LOCATION_TOOLCUT_H_
#define CLUSTER_LOCATION_TOOLCUT_H_

#include "../ClusterLocation.h"
namespace toolcut
{

class SCOToolCut;

class ClusterLocationToolCut
{
public:
    ClusterLocationToolCut(const volumedriver::ClusterLocation& loc);

    ClusterLocationToolCut(const std::string& name);

    static bool
    isClusterLocationString(const std::string& str);

    uint16_t
    offset();

    uint8_t
    version();

    uint8_t
    cloneID();

    volumedriver::SCONumber
    number();

    std::string
    str() const;

    SCOToolCut*
    sco();

    std::string
    repr() const;


private:
    volumedriver::ClusterLocation loc_;
};
}


#endif

// Local Variables: **
// mode: c++ **
// End: **
