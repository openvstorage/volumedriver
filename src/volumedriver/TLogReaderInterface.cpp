// Copyright 2015 Open vStorage NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
