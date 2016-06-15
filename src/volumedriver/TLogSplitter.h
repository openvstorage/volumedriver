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

#ifndef TLOGSPLITTER_H
#define TLOGSPLITTER_H
#include "Types.h"
#include "ScrubbingTypes.h"
#include <youtils/Logging.h>
#include "youtils/FileUtils.h"
#include "ScrubbingSCOData.h"
#include "TLogReaderInterface.h"
#include <boost/smart_ptr/shared_ptr.hpp>

namespace volumedriver
{
class FilePool;
class TLogWriter;
class Entry;
}

namespace scrubbing
{
namespace fs = boost::filesystem;
using namespace volumedriver;

class TLogSplitter
{
public:
    typedef std::map<uint64_t, fs::path> MapType;



    TLogSplitter(std::shared_ptr<TLogReaderInterface> reader_,
                 ScrubbingSCODataVector& sco_data,
                 const RegionExponent&,
                 volumedriver::FilePool& filepool);

    ~TLogSplitter();

    void
    operator()();


    DECLARE_LOGGER("TLogSplitter");

    const MapType &
    getMap() const
    {
        return tlog_names_;
    }

private:
    void
    doEntry(const volumedriver::Entry*);

    ClusterRegionSize region_exponent_;
    std::shared_ptr<volumedriver::TLogReaderInterface>  reader_;
    volumedriver::FilePool& filepool_;
    ScrubbingSCODataVector& sco_data_;

    typedef std::map<uint64_t, volumedriver::TLogWriter*> TLogWriterMap;

    MapType tlog_names_;


    TLogWriterMap tlog_writers_;

};
}

#endif // TLOGSPLITTER_

// Local Variables: **
// mode : c++ **
// End: **
