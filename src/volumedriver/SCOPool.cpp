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

#include "ClusterLocation.h"
#include "SCOPool.h"
#include "TLogReader.h"
#include "TLogWriter.h"

#include <youtils/Assert.h>

namespace scrubbing
{
using namespace volumedriver;

SCOPool::SCOPool(ScrubbingSCODataVector& scos,
                 const fs::path& metadatascrubbed_tlog,
                 FilePool& filepool,
                 BackendInterface* backendinterface,
                 const ClusterExponent& cluster_exponent,
                 // SCOSIZE in number of clusters
                 const uint64_t scosize,
                 uint16_t minimum_used_entries,
                 NormalizedSCOAccessData& access_data,
                 SCO lastSCOName,
                 std::vector<SCO>& new_scos)
    : scodata_(scos),
      filepool_(filepool),
      backendinterface_(backendinterface),
      cluster_size_(1<< cluster_exponent),
      sco_size_(scosize),
      buffer_(0),
      current_sco_(0),
      current_offset_(0),
      current_sco_name_(0),
      access_data_(access_data),
      metadatascrubbed_tlog_(metadatascrubbed_tlog),
      minimum_used_entries_(minimum_used_entries),
      backend_size_(0),
      new_scos_(new_scos),
      number_of_scos_read_from_backend(0),
      number_of_scos_written_to_backend(0)
{
    buffer_ = new char[cluster_size_];
    // Make sure the last SCO is not scrubbed away. This is needed to ensure that the failover
    // cache can be replayed without gaps in case or restart.
    for(ScrubbingSCODataVector::iterator  it = scodata_.begin(); it != scodata_.end(); ++it)
    {
        if(it->sconame_ == lastSCOName)
        {
            it->state = ScrubbingSCOData::State::NotScrubbed;
        }
        VERIFY(it->sconame_.number() <= lastSCOName.number());
    }

}

SCOPool::~SCOPool()
{
    delete[] (char*)buffer_;
    Deleter del;


    std::for_each(sconame_map.begin(),sconame_map.end(), del);
}

void
SCOPool::doEntry()
{
    ClusterLocation cluster_location = e->clusterLocation();
    backend_size_ += cluster_size_;

    SCO sco_name = cluster_location.sco();
    while((scodata_iterator_ != scodata_.end()) and
          (not (sco_name == scodata_iterator_->sconame_)))
    {

        if(scodata_iterator_->state == ScrubbingSCOData::State::Unknown)
        {
            VERIFY(scodata_iterator_->usageCount == 0);
            scodata_iterator_->state = ScrubbingSCOData::State::Scrubbed;
        }
        else
        {
            // We're leaving the old one. Make sure it's ok.
            VERIFY(current_usage_count_ == 0);
        }
        ++scodata_iterator_;
    }
    VERIFY(scodata_iterator_ != scodata_.end());
    VERIFY(scodata_iterator_->usageCount != 0);
    if(scodata_iterator_->state == ScrubbingSCOData::State::Unknown)
    {

        scodata_iterator_->state =
            scodata_iterator_->usageCount < minimum_used_entries_ ?
            ScrubbingSCOData::State::Scrubbed : ScrubbingSCOData::State::NotScrubbed;

        current_usage_count_ = scodata_iterator_->usageCount;
    }

    if(scodata_iterator_->state == ScrubbingSCOData::State::Scrubbed or
       scodata_iterator_->state == ScrubbingSCOData::State::Reused)
    {
        // Get the old sco from the backend -- this is now superflous as all the sco's are
        // ordered again as they come into the SCOPool...
        // We keep the name/fileio map now but close each file and remove it when it is not
        // needed any more. We may choose to simplify this.
        SCONameMap::iterator it = sconame_map.find(sco_name);
        FileDescriptor* in_io = 0;
        if(it == sconame_map.end())
        {
            // Here we do some caching???
            if(not sconame_map.empty())
            {
                try
                {
                    // sconame_map.rbegin()->second->close();
                }
                catch(std::exception& e)
                {
                    LOG_ERROR("Problem closing filedescriptor of " << sconame_map.rbegin()->first << " " << e.what());
                }
                catch(...)
                {
                    LOG_ERROR("Unknown problem closing filedescriptor of " << sconame_map.rbegin()->first);
                }

                delete sconame_map.rbegin()->second;
                sconame_map.rbegin()->second = 0;

                // Couldn't we clear the sconame_map here?
                try
                {
                    fs::path file = filepool_.directory() / sconame_map.rbegin()->first.str();
                    fs::remove(file);
                }
                catch(std::exception& e)
                {
                    LOG_ERROR("Ignoring problem removing "
                              << sconame_map.rbegin()->first
                              << " from the filepool, " << e.what());

                }
                catch(...)
                {
                    LOG_ERROR("Ignoring unknown problem removing "
                              << sconame_map.rbegin()->first << " from the filepool");
                }
            }

            std::string sco_string = sco_name.str();
            fs::path sco_path = filepool_.newFile(sco_string);
            backendinterface_->read(sco_path, sco_string, InsistOnLatestVersion::F);
            ++number_of_scos_read_from_backend;
            in_io = new FileDescriptor(sco_path, FDMode::Read);
            std::pair<SCONameMap::iterator, bool> res =
                sconame_map.insert(std::make_pair(sco_name, in_io));
            VERIFY(res.second);
            it = res.first;
        }
        else
        {
            // We do some extra asserting here that is to be removed after bart ran some more tests
            VERIFY(it->first == sconame_map.rbegin()->first);
            VERIFY(it->second != 0);
            in_io = it->second;
        }
        // read, write and put in tlogs

        uint64_t sco_offset = cluster_location.offset() * cluster_size_;
        in_io->pread(buffer_, cluster_size_, sco_offset);
        MaybeUpdateCurrentSCO();
        new_sco_access_data[current_sco_name_] += access_data_[sco_name];

        current_sco_->write(buffer_, cluster_size_);
        checksum_.update(buffer_, cluster_size_);
        // Entry new_entry(e->clusterAddress(),
        //                 ClusterLocation(current_sco_name_,
        //                (c                 current_offset_++));
        ClusterLocation loc(current_sco_name_,
                          current_offset_);
        ClusterLocationAndHash loc_and_hash(loc,
                                            e->clusterLocationAndHash().weed);

        rewritten_tlog_writer->add(e->clusterAddress(),
                                   loc_and_hash);
        relocations_tlog_writer->add(e->clusterAddress(),
                                     e->clusterLocationAndHash());
        relocations_tlog_writer->add(e->clusterAddress(),
                                     loc_and_hash);
        ++current_offset_;
    }
    else if(scodata_iterator_->state == ScrubbingSCOData::State::NotScrubbed)
    {
        nonrewritten_tlog_writer->add(e->clusterAddress(),
                                      e->clusterLocationAndHash());
    }
    else
    {
        VERIFY(0=="We should not get here");
    }
    --current_usage_count_;
}


std::pair<CheckSum, uint64_t>
SCOPool::operator()()
{
    nonrewritten_tlog_path_ = filepool_.newFile("nonrewritten_tlog");
    nonrewritten_tlog_writer.reset(new TLogWriter(nonrewritten_tlog_path_));
    rewritten_tlog_path_ = filepool_.newFile("rewritten_tlog");
    rewritten_tlog_writer.reset(new TLogWriter(rewritten_tlog_path_));
    relocations_tlog_path_ = filepool_.newFile("relocations_tlog");
    relocations_tlog_writer.reset(new TLogWriter(relocations_tlog_path_));

    TLogReader input_reader(metadatascrubbed_tlog_);

    scodata_iterator_ = scodata_.begin();
    to_be_reused_iterator_ = scodata_.begin();

    current_usage_count_ = 0;

    while((e = input_reader.nextLocation()))
    {
        doEntry();
    }
    VERIFY(scodata_iterator_ == scodata_.end() or
           ++scodata_iterator_ == scodata_.end());

    nonrewritten_tlog_writer.reset(0);
    rewritten_tlog_writer.reset(0);
    CheckSum ss = relocations_tlog_writer->getCheckSum();
    uint64_t relocationEntries = relocations_tlog_writer->getEntriesWritten() / 2;

    relocations_tlog_writer.reset(0);

    if(current_sco_)
    {
        current_sco_->sync();
    }

    if(current_offset_ > 0)
    {
        LOG_INFO("setting sco access data for " << current_sco_name_
                 << " to " << new_sco_access_data[current_sco_name_] << " / "  <<current_offset_);
        new_sco_access_data[current_sco_name_] /= current_offset_;
        fs::path new_sco_path = current_sco_->path();
        ++number_of_scos_written_to_backend;
        backendinterface_->write(new_sco_path,
                                 current_sco_name_.str(),
                                 OverwriteObject::F,
                                 &checksum_);
        new_scos_.push_back(current_sco_name_);

    }

    delete current_sco_;
    current_sco_ = 0;
    return std::pair<CheckSum, uint64_t>(ss, relocationEntries);
}


static inline
void
nextVersion(SCOVersion& version)
{
    if(version == 255)
    {
        version = SCOVersion(0);
    }
    else
    {
         ++version;
    }
}

SCO
SCOPool::makeNewSCOName(const SCO in) const
{
    SCOVersion version = in.version();

    for(uint8_t i = 0; i < 255; ++i)
    {
        nextVersion(version);
        SCO sconame (in.number(),
                     in.cloneID(),
                     version);

        if(not backendinterface_->objectExists(sconame.str()))
        {
            LOG_DEBUG("created a new sco" << sconame);
            return sconame;
        }
    }
    LOG_ERROR("Could not create a new SCOName for " << in);
    throw fungi::IOException("Could not create a new SCOName");

}

struct SCONameFinder
{
    SCONameFinder(const SCO sconame)
        :sconame_(sconame)
    {
    }

    bool
    operator()(const ScrubbingSCOData& i) const
    {
        return sconame_ == i.sconame_;
    }
    SCO sconame_;

};


void
SCOPool::MaybeUpdateCurrentSCO()
{
    // Do an or here??
    if(not current_sco_)
    {
        current_offset_ = 0;
        while(to_be_reused_iterator_->state != ScrubbingSCOData::State::Scrubbed
              and to_be_reused_iterator_ != scodata_.end())
        {
            ++to_be_reused_iterator_;
        }
        VERIFY(to_be_reused_iterator_ != scodata_.end());
        VERIFY(to_be_reused_iterator_ <= scodata_iterator_);
        to_be_reused_iterator_->state = ScrubbingSCOData::State::Reused;
        current_sco_name_ = makeNewSCOName(to_be_reused_iterator_->sconame_);
        std::string new_sco_string = current_sco_name_.str();
        fs::path  new_sco_path = filepool_.newFile(new_sco_string);
        current_sco_ = new FileDescriptor(new_sco_path,FDMode::Write);
        checksum_.reset();

    }
    else if(current_offset_ >= sco_size_)
    {
        fs::path old_sco_path = current_sco_->path();
        current_sco_->sync();
        delete current_sco_;
        VERIFY(sco_size_ > 0);

        LOG_INFO("setting sco access data for " << current_sco_name_
                 << " to " << new_sco_access_data[current_sco_name_]
                 << " / " <<  sco_size_);
        new_sco_access_data[current_sco_name_] /= sco_size_;
        ++number_of_scos_written_to_backend;
        backendinterface_->write(old_sco_path,
                                 current_sco_name_.str(),
                                 OverwriteObject::F,
                                 &checksum_);
        new_scos_.push_back(current_sco_name_);
        fs::remove(filepool_.directory() / current_sco_name_.str());

        current_offset_ = 0;
        while(to_be_reused_iterator_->state != ScrubbingSCOData::State::Scrubbed
              and to_be_reused_iterator_ != scodata_.end())
        {
            ++to_be_reused_iterator_;
        }
        VERIFY(to_be_reused_iterator_ != scodata_.end());
        VERIFY(to_be_reused_iterator_ <= scodata_iterator_);
        to_be_reused_iterator_->state = ScrubbingSCOData::State::Reused;
        current_sco_name_ = makeNewSCOName(to_be_reused_iterator_->sconame_);
        std::string new_sco_string = current_sco_name_.str();
        fs::path  new_sco_path = filepool_.newFile(new_sco_string);
        current_sco_ = new FileDescriptor(new_sco_path,FDMode::Write);
        checksum_.reset();

    }
}

}
// Local Variables: **
// End: **
