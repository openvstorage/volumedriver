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

#ifndef SCO_TOOL_CUT_H
#define SCO_TOOL_CUT_H
#include "../SCO.h"

namespace volumedriver
{

namespace python
{

class SCOToolCut
{
public:
    SCOToolCut(const volumedriver::SCO& sco);

    SCOToolCut(const std::string& i_str);

    std::string
    str() const;

    uint8_t
    version() const;


    uint8_t
    cloneID() const;

    volumedriver::SCONumber
    number() const;

    static bool
    isSCOString(const std::string& str);

    bool
    asBool() const;

private:
    volumedriver::SCO sco_;

};

}

}

#endif // SCO_TOOL_CUT_H

// Local Variables: **
// mode: c++ **
// End: **
