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

#ifndef ENTRY_TOOLCUT_H_
#define ENTRY_TOOLCUT_H_

#include "../Entry.h"
#include "ClusterLocationToolCut.h"

#include <youtils/Logging.h>

namespace volumedriver
{

namespace python
{

class EntryToolCut
{
    DECLARE_LOGGER("EntryToolCut")
public:
    EntryToolCut(const volumedriver::Entry* entry)
        :entry_(entry)
    {
        VERIFY(entry_);
    }

    volumedriver::Entry::Type
    type() const;

    volumedriver::CheckSum::value_type
    getCheckSum() const;

    volumedriver::ClusterAddress
    clusterAddress() const;

    ClusterLocationToolCut
    clusterLocation() const;

    std::string
    str() const;

    std::string
    repr() const;

private:
    const volumedriver::Entry* entry_;
};

}

}

#endif

// Local Variables: **
// mode: c++ **
// End: **
