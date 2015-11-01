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
                           RegionExponent regionsize,
                           ClusterExponent clustersize)
    : iterator_(iterator)
    , clustersize_(clustersize)
    , regionsize_(regionsize)
    , scodata_(scodata)
    , filepool_(filepool)
{
    cluster_begin_ = (iterator_->first << regionsize_);
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
    bitset_type bitset(1 << regionsize_);

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
        VERIFY(e->clusterAddress() - cluster_begin_ < (1UL<<regionsize_));

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
