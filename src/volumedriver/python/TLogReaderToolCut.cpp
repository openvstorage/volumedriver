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

#include <boost/foreach.hpp>
#include <boost/python/dict.hpp>

#include "EntryToolCut.h"
#include "TLogReaderToolCut.h"

#include "../ClusterLocation.h"
#include "../EntryProcessor.h"
#include "../SCO.h"
#include "../TLog.h"
#include "../TLogReader.h"

#include <youtils/CheckSum.h>

// #include <backend-python/ConnectionInterface.h>

namespace volumedriver
{

namespace python
{

boost::python::object
TLogReaderToolCut::object_;

TLogReaderToolCut::TLogReaderToolCut(const fs::path& path)
    : path_(path)
{
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
        result.append(sco);
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
                          lh.clusterLocation);
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

}
// Local Variables: **
// mode: c++ **
// End: **
