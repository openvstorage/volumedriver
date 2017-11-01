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

#include "ClusterLocationToolCut.h"
#include <iostream>

namespace volumedriver
{

namespace python
{

ClusterLocationToolCut::ClusterLocationToolCut(const volumedriver::ClusterLocation& loc)
    : loc_(loc)
{}


ClusterLocationToolCut::ClusterLocationToolCut(const std::string& name)
    : loc_(name)
{}

bool
ClusterLocationToolCut::isClusterLocationString(const std::string& str)
{
    return volumedriver::ClusterLocation::isClusterLocationString(str);
}

uint16_t
ClusterLocationToolCut::offset()
{
    return loc_.offset();
}

uint8_t
ClusterLocationToolCut::version()
{
    return loc_.version();
}

uint8_t
ClusterLocationToolCut::cloneID()
{
    return loc_.cloneID();
}

volumedriver::SCONumber
ClusterLocationToolCut::number()
{
    return loc_.number();
}

std::string
ClusterLocationToolCut::str() const
{
    return loc_.str();
}

volumedriver::SCO
ClusterLocationToolCut::sco()
{
    return loc_.sco();
}

std::string
ClusterLocationToolCut::repr() const
{
    return str();
}

}

}

// Local Variables: **
// mode: c++ **
// End: **
