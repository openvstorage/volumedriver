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

#include "TLogSplitter.h"
#include "CombinedTLogReader.h"
#include <boost/shared_ptr.hpp>
#include "TLogWriter.h"
#include "FilePool.h"

namespace scrubbing
{
using namespace volumedriver;

TLogSplitter::TLogSplitter(std::shared_ptr<TLogReaderInterface> reader,
                           ScrubbingSCODataVector& sco_data,
                           const RegionExponent& region_exponent,
                           FilePool& filepool)
    : region_exponent_(region_exponent),
      reader_(reader),
      filepool_(filepool),
      sco_data_(sco_data)
{}

TLogSplitter::~TLogSplitter()
{

    for(TLogWriterMap::iterator it = tlog_writers_.begin() ; it != tlog_writers_.end(); ++it)
    {
        delete it->second;
    }
    tlog_writers_.clear();
}

void
TLogSplitter::doEntry(const Entry* e)
{
    ClusterAddress cluster_address = e->clusterAddress();
    uint64_t index = cluster_address >> region_exponent_;
    TLogWriterMap::const_iterator it = tlog_writers_.find(index);
    TLogWriter* tlog_writer = 0;
    if(it == tlog_writers_.end())
    {
        std::stringstream ss;
        ss << "tlog_for_region_" << index;
        fs::path tlog_path = filepool_.newFile(ss.str());
        tlog_writer = new TLogWriter(tlog_path);
        tlog_names_.insert(std::make_pair(index, tlog_path));
        tlog_writers_.insert(std::make_pair(index, tlog_writer));
    }
    else
    {
        tlog_writer = it->second;
    }

    tlog_writer->add(e->clusterAddress(),
                     e->clusterLocationAndHash());

    SCO sco_name = e->clusterLocation().sco();
    if(sco_data_.empty() or
       (not (sco_name == sco_data_.back().sconame_)))
    {
        ScrubbingSCOData s(sco_name);

        sco_data_.push_back(s);
    }
    else
    {
        ++(sco_data_.back().size);
    }
}

void
TLogSplitter::operator()()
{
    const Entry* e;

    while((e = reader_->nextLocation()))
    {
        doEntry(e);
    }
    for(TLogWriterMap::iterator it = tlog_writers_.begin() ; it != tlog_writers_.end(); ++it)
    {
        delete it->second;
    }
    tlog_writers_.clear();
}
}

// Local Variables: **
// End: **
