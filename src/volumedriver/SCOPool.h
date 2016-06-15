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

#ifndef SCOPOOL_H
#define SCOPOOL_H

#include "Entry.h"
#include "FilePool.h"
#include "NormalizedSCOAccessData.h"
#include "ScrubbingTypes.h"
#include "TLogSplitter.h"

#include <youtils/FileDescriptor.h>
#include <youtils/CheckSum.h>

#include <backend/BackendInterface.h>

namespace scrubbing
{

class SCOPool
{
public:
    SCOPool(scrubbing::ScrubbingSCODataVector& scos_,
            const boost::filesystem::path& metadatascrubbed_tlog,
            volumedriver::FilePool& filepool,
            volumedriver::BackendInterface& backendinterface,
            const volumedriver::ClusterExponent& cluster_exponent,
            const uint64_t scosize,
            uint16_t minimum_number_of_clusters_in_sco,
            NormalizedSCOAccessData& norm_access_data,
            volumedriver::SCO lastSCONumber,
            std::vector<volumedriver::SCO>& new_scos);

    ~SCOPool() = default;

    // Returns the checksum of the relocations tlog
    std::pair<youtils::CheckSum, uint64_t>
    operator()();

    const boost::filesystem::path&
    nonrewritten_tlog_path() const
    {
        return nonrewritten_tlog_path_;
    }

    const boost::filesystem::path&
    rewritten_tlog_path() const
    {
        return rewritten_tlog_path_;
    }

    const boost::filesystem::path&
    relocations_tlog_path() const
    {
        return relocations_tlog_path_;
    }

    uint64_t
    get_backend_size() const
    {
        return backend_size_;
    }

    void
    getSCONamesToBeDeleted(std::vector<volumedriver::SCO>& out) const
    {
        for(ScrubbingSCODataVector::const_iterator it = scodata_.begin();
            it != scodata_.end();
            ++it)
         {
             VERIFY(it->state != scrubbing::ScrubbingSCOData::State::Unknown);

             if(it->state == scrubbing::ScrubbingSCOData::State::Scrubbed or
                it->state == scrubbing::ScrubbingSCOData::State::Reused)
             {
                 out.push_back(it->sconame_);
             }
         }
    }

    void
    get_access_data(std::vector<std::pair<volumedriver::SCO, float> >& access_data)
    {
        for(std::map<volumedriver::SCO, float>::const_iterator it = new_sco_access_data.begin();
            it != new_sco_access_data.end();
            ++it)
        {
            access_data.push_back(*it);
        }
    }

    uint64_t
    numberOfSCOSReadFromBackend() const
    {
        return number_of_scos_read_from_backend;
    }

    uint64_t
    numberOfSCOSWrittenToBackend() const
    {
        return number_of_scos_written_to_backend;
    }

 private:
    DECLARE_LOGGER("SCOPool");

    boost::filesystem::path nonrewritten_tlog_path_;
    std::unique_ptr<volumedriver::TLogWriter> nonrewritten_tlog_writer;
    boost::filesystem::path rewritten_tlog_path_;
    std::unique_ptr<volumedriver::TLogWriter> rewritten_tlog_writer;
    boost::filesystem::path relocations_tlog_path_;
    std::unique_ptr<volumedriver::TLogWriter> relocations_tlog_writer;

    ScrubbingSCODataVector::iterator scodata_iterator_;

    ScrubbingSCODataVector::iterator to_be_reused_iterator_;

    ScrubbingSCODataVector& scodata_;
    volumedriver::FilePool& filepool_;
    volumedriver::BackendInterface& backendinterface_;

    using SCONameMap =
        std::map<volumedriver::SCO, std::unique_ptr<youtils::FileDescriptor>>;

    SCONameMap sconame_map_;
    const uint64_t cluster_size_;
    const uint64_t sco_size_;
    std::vector<uint8_t> buffer_;

    std::unique_ptr<youtils::FileDescriptor> current_sco_;
    volumedriver::SCOOffset current_offset_;
    volumedriver::SCO current_sco_name_;

    uint16_t current_usage_count_;
    NormalizedSCOAccessData& access_data_;

    volumedriver::CheckSum checksum_;

    const boost::filesystem::path metadatascrubbed_tlog_;

    const uint16_t minimum_used_entries_;

    uint64_t backend_size_;

    std::map<volumedriver::SCO, float> new_sco_access_data;

    std::vector<volumedriver::SCO> new_scos_;
    uint64_t number_of_scos_read_from_backend;
    uint64_t number_of_scos_written_to_backend;

    void
    doEntry(const volumedriver::Entry& e);

    volumedriver::SCO
    makeNewSCOName(const volumedriver::SCO in) const;

    void
    MaybeUpdateCurrentSCO();
};

}

#endif // SCOPOOL_H
// Local Variables: **
// End: **
