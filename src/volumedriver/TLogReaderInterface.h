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

#ifndef TLOGREADERINTERFACE_H
#define TLOGREADERINTERFACE_H
#include "Entry.h"
#include <vector>
#include "SCO.h"
#include <youtils/IOException.h>

namespace volumedriver
{
VD_BOOLEAN_ENUM(KeepRunningCRC);
VD_BOOLEAN_ENUM(CheckSCOCRC);

class TLogReaderInterface
{
public:
    virtual ~TLogReaderInterface() = default;

    virtual const Entry*
    nextAny() = 0;

    ClusterLocation
    nextClusterLocation();

    const Entry*
    nextLocation();

    void
    SCONames(std::vector<SCO>& out);

    void
    SCONamesAndChecksums(std::map<SCO, CheckSum>& out_checksums);

    // Revisit this: would be much nicer if we could specify the member function to be called
    // and default it to processEntry but for some reason that doesn't work.
    template<typename T>
    void
    for_each(T& t)
    {
        const Entry* e = 0;
        while((e = nextAny()))
        {
            t.processEntry(e);
        }
    }

    using Ptr = std::unique_ptr<TLogReaderInterface>;
};

}

#endif // TLOGREADERINTERFACE_H



// Local Variables: **
// mode: c++ **
// End: **
