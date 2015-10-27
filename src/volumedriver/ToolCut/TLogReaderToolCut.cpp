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

#include <boost/foreach.hpp>
#include <boost/python/dict.hpp>

#include "../EntryProcessor.h"
#include "TLogReaderToolCut.h"
#include "../TLogReader.h"
#include "../TLog.h"
#include <youtils/CheckSum.h>
#include "../ClusterLocation.h"
#include "SCOToolCut.h"
#include "ClusterLocationToolCut.h"
#include "EntryToolCut.h"
// #include <backend-python/ConnectionInterface.h>
namespace toolcut
{
boost::python::object
TLogReaderToolCut::object_;

TLogReaderToolCut::TLogReaderToolCut(const fs::path& path)
    : path_(path)
{
    tlog_reader_.reset(new vd::TLogReader(path));
}


TLogReaderToolCut::TLogReaderToolCut(boost::python::object& backend,
                                     const std::string& nspace,
                                     const std::string& tlog_name,
                                     const fs::path& path)
    : path_(path)
{
    backend.attr("read")(nspace,
                         path.string(),
                         tlog_name,
                         true);
    tlog_reader_.reset(new vd::TLogReader(path));
}


bpy::list
TLogReaderToolCut::SCONames() const
{
    bpy::list result;
    std::vector<vd::SCO> scos;
    tlog_reader_->SCONames(scos);
    BOOST_FOREACH(vd::SCO sco, scos)
    {
        result.append(SCOToolCut(sco));
    }
    return result;
}

namespace
{

using namespace volumedriver;
struct TLogCallBacker : public BasicDispatchProcessor
{
    TLogCallBacker(const boost::python::object& ClusterEntry,
                   const boost::python::object& TLogCRCEntry ,
                   const boost::python::object& SCOCRC,
                   const boost::python::object& SyncTC)
        : ClusterEntry_(ClusterEntry)
        , TLogCRCEntry_(TLogCRCEntry)
        , SCOCRC_(SCOCRC)
        , SyncTC_(SyncTC)
    {}

    void
    processLoc(ClusterAddress ca,
                const ClusterLocationAndHash& lh)
    {
        if(not ClusterEntry_.is_none())
        {
            // Do we wanna pass the weeds here... will be slow??
            ClusterEntry_(ca,
                          ClusterLocationToolCut(lh.clusterLocation));
        }
    }

    void
    processTLogCRC(CheckSum::value_type chksum)
    {
        if(not TLogCRCEntry_.is_none())
        {
            TLogCRCEntry_(chksum);
        }
    }

    void
    processSCOCRC(CheckSum::value_type chksum)
    {
        if(not SCOCRC_.is_none())
        {
            SCOCRC_(chksum);
        }
    }


    void
    processSync()
    {
        if(not SyncTC_.is_none())
        {
            SyncTC_();
        }

    }

private:
    const boost::python::object& ClusterEntry_;
    const boost::python::object& TLogCRCEntry_;
    const boost::python::object& SCOCRC_;
    const boost::python::object& SyncTC_;

};
}

void
TLogReaderToolCut::rewind()
{
    tlog_reader_.reset(new TLogReader(path_));
}


bool
TLogReaderToolCut::isTLog(const std::string& tlog)
{
    return vd::TLog::isTLogString(tlog);
}

void
TLogReaderToolCut::forEach(const boost::python::object& ClusterEntry,
                           const boost::python::object& TLogCRCEntry ,
                           const boost::python::object& SCOCRC,
                           const boost::python::object& SyncTC)
{
    try
    {

        TLogCallBacker tlog_callbacker(ClusterEntry,
                                       TLogCRCEntry,
                                       SCOCRC,
                                       SyncTC);

        tlog_reader_->for_each(tlog_callbacker);
    }
    catch(...)
    {
        tlog_reader_.reset(new TLogReader(path_));
        throw;
    }
    tlog_reader_.reset(new TLogReader(path_));
}

void
TLogReaderToolCut::forEachEntry(const boost::python::object& callback)
{
    try
    {
        const Entry* e = 0;
        while((e = tlog_reader_->nextAny()))
        {
            EntryToolCut entry(e);
            callback(entry);
        }
    }
    catch(...)
    {
        tlog_reader_.reset(new TLogReader(path_));
        throw;

    }
    tlog_reader_.reset(new TLogReader(path_));
}

std::string
TLogReaderToolCut::str() const
{
    return "ToolCut.TLogReader for " + path_.string();

}

std::string
TLogReaderToolCut::repr() const
{
    return "< ToolCut.TLogReader for " + path_.string() + " >";

}


}

// Local Variables: **
// mode: c++ **
// End: **
