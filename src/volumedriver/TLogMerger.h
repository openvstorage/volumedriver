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

#ifndef TLOG_MERGER_H
#define TLOG_MERGER_H
#include "FilePool.h"
#include "youtils/FileUtils.h"
#include <youtils/Assert.h>
#include "TLogWriter.h"
#include "BackwardTLogReader.h"
#include "TLogReader.h"
#include "ClusterLocation.h"
#include <youtils/CheckSum.h>
namespace scrubbing
{
using namespace volumedriver;
namespace fs = boost::filesystem;

template<typename T>
class TLogMerger
{
private:
    typedef std::list<std::pair<T*, const Entry*> > TLogReaderList;
public:
    TLogMerger()
    {}


    ~TLogMerger()
    {
    }


    void addTLogReader(T* t)
    {

        const Entry* e = t->nextLocation();

        if(e == 0)
        {
            delete t;
            return;
        }

        tlog_readers.push_back(std::make_pair(t, e));
    }



    CheckSum
    operator()(const fs::path& out,
               ClusterLocation* last = 0)
    {
        TLogWriter tlog_writer(out);

        const Entry* e;
        while((e = next()))
        {
            tlog_writer.add(e->clusterAddress(),
                            e->clusterLocationAndHash());
        }

        VERIFY(tlog_readers.empty());
        // Y42: tlog_writer.close();
         if(last)
         {
             *last = tlog_writer.getClusterLocation();

             VERIFY(last or
                    fs::file_size(out) == 0);
         }

        return tlog_writer.getCheckSum();
    }

    DECLARE_LOGGER("TKogMerger");

private:
    TLogReaderList tlog_readers;

    typename TLogReaderList::iterator
    compare(typename TLogReaderList::iterator first, typename TLogReaderList::iterator second)
    {
        SCONumber fst = first->second->clusterLocation().number();
        SCONumber snd = second->second->clusterLocation().number();
        if(fst < snd)
        {
            return first;
        }
        else if(snd < fst)
        {
            return second;
        }
        else
        {
            SCOOffset fst_o = first->second->clusterLocation().offset();
            SCOOffset snd_o = second->second->clusterLocation().offset();
            return fst_o < snd_o ? first : second;
        }
    }


    const Entry*
    next()
    {
        if(tlog_readers.empty())
        {
            return 0;
        }
        else
        {
            typename TLogReaderList::iterator t = tlog_readers.begin();


            for(typename TLogReaderList::iterator i = ++(tlog_readers.begin()); i != tlog_readers.end(); ++i)
            {
                t = compare(t, i);
            }
            entry = *t->second;

            t->second = t->first->nextLocation();

            if(t->second == 0)
            {
                delete t->first;
                tlog_readers.erase(t);
            }
        }
        return &entry;
    }

    Entry entry;
    CheckSum checksum;
};

typedef TLogMerger<BackwardTLogReader> BackwardTLogMerger;
typedef TLogMerger<TLogReader> ForwardTLogMerger;

}

#endif // TLOG_MERGER_H

// Local Variables: **
// End: **
