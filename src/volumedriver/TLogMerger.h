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
