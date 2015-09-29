// Copyright 2015 Open vStorage NV
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

#ifndef ENTRYPROCESSOR_H_
#define ENTRYPROCESSOR_H_

#include "TLogReaderInterface.h"

namespace volumedriver
{

MAKE_EXCEPTION(SCOSwitchWithoutSCOCRC, fungi::IOException);
MAKE_EXCEPTION(SCOCRCWithoutActiveSCO, fungi::IOException);
MAKE_EXCEPTION(TLogWrongCRC, fungi::IOException);

template<typename T1,
         typename T2>
class CombinedProcessor
{
public:
    CombinedProcessor(T1& p1,
                      T2& p2)
        : p1_(p1)
        , p2_(p2)
    {}

    void processEntry(const Entry* e)
    {
        p1_.processEntry(e);
        p2_.processEntry(e);
    }

private:
    T1& p1_;
    T2& p2_;
};

template<typename T1,
         typename T2>
CombinedProcessor<T1,T2>
make_combined_processor(T1& t1, T2& t2)
{
    return CombinedProcessor<T1, T2>(t1, t2);
}

class BasicDispatchProcessor
{
public:
    BasicDispatchProcessor()
    {}

    virtual ~BasicDispatchProcessor()
    {}

    //override if necessary, don't forget to call dispatch
    void
    processEntry(const Entry* e)
    {
        dispatch(e);
    }

protected:
    void
    dispatch(const Entry* e)
    {
        if(e->isLocation())
        {
            processLoc(e->clusterAddress(),
                       e->clusterLocationAndHash());
        }
        else if(e->isTLogCRC())
        {
            processTLogCRC(e->getCheckSum());
        }
        else if(e->isSCOCRC())
        {
            processSCOCRC(e->getCheckSum());
        }
        else if(e->isSync())
        {
            processSync();
        }
        else
        {
            LOG_FATAL("Unknown Entry in TLOG, this should not happen!");
            throw fungi::IOException("You are not supposed to be here. Leave immediately through an error path");
        }
    }

    virtual void
    processLoc(ClusterAddress /*a*/,
                const ClusterLocationAndHash& /*loc_and_hash*/) = 0;

    virtual void
    processTLogCRC(CheckSum::value_type /*t*/) = 0;

    virtual void
    processSCOCRC(CheckSum::value_type /*t*/) = 0;

    virtual void
    processSync() = 0;

    DECLARE_LOGGER("Dispatcher");
};

class ClusterLocationProcessor
    : public BasicDispatchProcessor
{
public:
    ClusterLocationProcessor()
    {}

    virtual ~ClusterLocationProcessor(){}

    virtual void
    processLoc(ClusterAddress /*a*/,
               const ClusterLocationAndHash& loc_and_hash)
    {
        current_clusterlocation_ = loc_and_hash.clusterLocation;
    }

    virtual void
    processTLogCRC(CheckSum::value_type /*t*/)
    {}

    virtual void
    processSCOCRC(CheckSum::value_type /*t*/)
    {}

    virtual void
    processSync()
    {}

    ClusterLocation current_clusterlocation_;
};

class CheckSCOCRCProcessor
    : public ClusterLocationProcessor
{
public:
    CheckSCOCRCProcessor()
        : seen_final_sco_crc_(true)
    {}

    virtual ~CheckSCOCRCProcessor()
    {}

    virtual void
    processLoc(ClusterAddress a,
                const ClusterLocationAndHash& loc_and_hash)
    {
        if(current_clusterlocation_.sco() != loc_and_hash.clusterLocation.sco() and
                not current_clusterlocation_.isNull() and
                not seen_final_sco_crc_)
        {
            throw SCOSwitchWithoutSCOCRC("New sco started without sco crc for the previous one");
        }

        seen_final_sco_crc_ = false;
        //At the end:
        //- clusterlocation only updated here
        //- entries only updated here
        ClusterLocationProcessor::processLoc(a, loc_and_hash);
    }

    virtual void
    processSCOCRC(CheckSum::value_type t)
    {
        if(current_clusterlocation_.isNull())
        {
            throw SCOCRCWithoutActiveSCO("SCO CRC Without Active SCO");
        }

        seen_final_sco_crc_ = true;

        //At the end:
        //- clusterlocation only updated here
        //- entries only updated here
        ClusterLocationProcessor::processSCOCRC(t);
    }

    bool seen_final_sco_crc_;
};

class CheckTLogAndSCOCRCProcessor
    : public CheckSCOCRCProcessor
{
public:
    CheckTLogAndSCOCRCProcessor(const std::string tlogname)
        : last_entry_was_tlog_crc_(false)
        , tlogname_(tlogname)
        , num_entries_(0)
    {}

    void
    processEntry(const Entry* e)
    {
        dispatch(e);
        last_entry_was_tlog_crc_ = (e->type() == Entry::Type::TLogCRC);
        checksum_.update(e, Entry::getDataSize());
        num_entries_++;
    }

    void
    processTLogCRC(CheckSum::value_type t)
    {
        LOG_INFO("Processing TLOG CRC");
        if(t != checksum_.getValue())
        {
            LOG_FATAL("Calculated TLog checksum doesn't match with checksum entry");
            throw TLogWrongCRC("Tlog has wrong crc entry");
        }
        return CheckSCOCRCProcessor::processTLogCRC(t);
    }

    bool last_entry_was_tlog_crc_;
    CheckSum checksum_;

    const
    std::string& tlogname() const
    {
        return tlogname_;
    }

    const std::string tlogname_;
    uint64_t num_entries_;
};

}

#endif /* ENTRYPROCESSOR_H_ */

// Local Variables: **
// mode: c++ **
// End: **
