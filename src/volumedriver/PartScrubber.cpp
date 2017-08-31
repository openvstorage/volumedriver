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

#include "PartScrubber.h"
#include "TLogReader.h"
#include "TLogWriter.h"
#include "BackwardTLogReader.h"

#include <cassert>

#include <boost/dynamic_bitset.hpp>

namespace scrubbing
{
using namespace volumedriver;

// Not so good since it's coupled too tightly to TLogSplitter
PartScrubber::PartScrubber(TLogSplitter::MapType::const_iterator& iterator,
                           scrubbing::ScrubbingSCODataVector& scodata,
                           FilePool& filepool,
                           RegionExponent region_exponent)
    : iterator_(iterator)
    , region_exponent_(region_exponent)
    , scodata_(scodata)
    , filepool_(filepool)
{
    VERIFY(region_exponent_ < (sizeof(unsigned long) * 8));
    cluster_begin_ = (iterator_->first << region_exponent_);
}

void
PartScrubber::updateIterator(const SCO sconame)
{
    while((scodata_iterator != scodata_.rend()) and
          (not (scodata_iterator->sconame_ == sconame)))
    {
       scodata_iterator++;
    }
    if(scodata_iterator == scodata_.rend())
    {
        LOG_ERROR("Unknown SCO in SCODATA: " << sconame);
        throw fungi::IOException("Unknown SCO in SCODATA");
    }
    else
    {
        ++(scodata_iterator->usageCount);
        VERIFY(scodata_iterator->size >= scodata_iterator->usageCount);
    }
}

void
PartScrubber::operator()(std::vector<fs::path>& tlogs)
{
    BackwardTLogReader tlog_reader(iterator_->second);

    typedef boost::dynamic_bitset<> bitset_type;

    // Y42 TODO check stack size influence on this...!
    // Do we want to alloc this on the heap???
    bitset_type bitset(1UL << region_exponent_);

    static_assert(sizeof(bitset_type::block_type) == 8,
                  "strange size for block type");
    static_assert(bitset_type::bits_per_block == 64,
                  "strange size for bits per block");

    // Start Looking at the back of the sconames.

    scodata_iterator = scodata_.rbegin();
    std::stringstream ss;
    ss << "metadatascrubbed_tlog_for_region_" << iterator_->first;
    fs::path tlog_path = filepool_.newFile(ss.str());

    TLogWriter out_tlog(tlog_path);
    ss.clear();

    const Entry* e;
    while((e = tlog_reader.nextLocation()))
    {
        // switch(e->getType())
        // {
        // case Entry::LOC:
        VERIFY(e->clusterAddress() - cluster_begin_ < (1UL<<region_exponent_));

        if(not bitset[e->clusterAddress() - cluster_begin_])
        {
            // We know it's a location entry here
            out_tlog.add(e->clusterAddress(),
                         e->clusterLocationAndHash());
            updateIterator(e->clusterLocation().sco());
            bitset[e->clusterAddress() - cluster_begin_] = true;
        }

        //     break;
        // default:
        //     LOG_ERROR("Unknown entry type in tlog, exiting");
        //     throw fungi::IOException("Unknown entry type");
        // }
    }
    tlogs.push_back(tlog_path);
}

}

// Local Variables: **
// End: **
