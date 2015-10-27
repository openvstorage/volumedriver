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
