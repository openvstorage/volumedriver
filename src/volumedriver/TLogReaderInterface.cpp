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

#include "TLogReaderInterface.h"

#include "ClusterLocation.h"
#include "EntryProcessor.h"
namespace volumedriver
{

void
TLogReaderInterface::SCONames(std::vector<SCO>& out)
{
    SCO prevContainer;

    const Entry* e;

    while ((e = nextLocation())){
        const ClusterLocation l = e->clusterLocation();
        const SCO tmp = l.sco();
        if(not (tmp == prevContainer))
        {
            out.push_back(tmp);
            prevContainer = tmp;
        }
    }
}
namespace
{
// Makes only sense for local tlogs, scrubbed tlogs have scocrc's without much meaning
struct SCONamesAndCheckSumsHelper : public CheckSCOCRCProcessor
{
    SCONamesAndCheckSumsHelper(std::map<SCO, CheckSum>& check_sums)
        : checksums_(check_sums)
    {}

    void
    processSCOCRC(CheckSum::value_type cs)
    {
        CheckSCOCRCProcessor::processSCOCRC(cs);
        checksums_[current_clusterlocation_.sco()] = CheckSum(cs);
    }

    std::map<SCO, CheckSum>& checksums_;
};
}


void
TLogReaderInterface::SCONamesAndChecksums(std::map<SCO, CheckSum>& out_checksums)
{
    SCONamesAndCheckSumsHelper sconames_and_checksums(out_checksums);
    for_each(sconames_and_checksums);
}

ClusterLocation
TLogReaderInterface::nextClusterLocation()
{
    const Entry* e = nextLocation();
    return e ? e->clusterLocation() : ClusterLocation();
}

const Entry*
TLogReaderInterface::nextLocation()
{
    const Entry* e;
    do {
        e = nextAny();
    }
    while (e and not e->isLocation());
    return e;
}


}
// Local Variables: **
// mode: c++ **
// End: **
