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

#ifndef PARTSCRUBBER_H
#define PARTSCRUBBER_H

#include "FilePool.h"
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
