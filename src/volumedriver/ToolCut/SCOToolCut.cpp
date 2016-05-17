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

#include "SCOToolCut.h"

namespace toolcut
{
SCOToolCut::SCOToolCut(const volumedriver::SCO& sco)
    :sco_(sco)
{}

SCOToolCut::SCOToolCut(const std::string& i_str)
    :sco_(i_str)
{}

std::string
SCOToolCut::str() const
{
    return sco_.str();
}

uint8_t
SCOToolCut::version() const
{
    return sco_.version();
}


uint8_t
SCOToolCut::cloneID() const
{
    return sco_.cloneID();
}


volumedriver::SCONumber
SCOToolCut::number() const
{
    return sco_.number();
}


bool
SCOToolCut::isSCOString(const std::string& str)
{
    return volumedriver::SCO::isSCOString(str);
}


bool
SCOToolCut::asBool() const
{
    return sco_.asBool();
}
}

// Local Variables: **
// mode: c++ **
// End: **
