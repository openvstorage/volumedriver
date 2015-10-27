// Copyright 2015 iNuron NV
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

#ifndef PARTSCRUBBER_H
#define PARTSCRUBBER_H

#include "SCOPool.h"
#include "TLogSplitter.h"
#include <string>

namespace scrubbing
{
class PartScrubber
{
public:
    PartScrubber(TLogSplitter::MapType::const_iterator&,
                 scrubbing::ScrubbingSCODataVector& scodata,
                 volumedriver::FilePool& filepool,
                 RegionExponent regionsize,
                 volumedriver::ClusterExponent clustersize);

    void
    operator()(std::vector<fs::path>& tlogs);

private:
    DECLARE_LOGGER("PartScrubber");

    ScrubbingSCODataVector::reverse_iterator scodata_iterator;

    TLogSplitter::MapType::const_iterator& iterator_;
    // What the F?
    const uint16_t clustersize_;
    RegionExponent regionsize_;
    //    SCOPool& scopool_;
    scrubbing::ScrubbingSCODataVector& scodata_;

    volumedriver::FilePool& filepool_;
    volumedriver::ClusterAddress cluster_begin_;

    void
    updateIterator(const volumedriver::SCO sconame);

    bool
    EntryNeedsWork(const volumedriver::Entry* e);
};
}

#endif // PartScrubber

// Local Variables: **
// End: **
