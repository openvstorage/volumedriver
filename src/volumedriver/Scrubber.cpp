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

#include "CombinedTLogReader.h"
#include "FilePool.h"
#include "NormalizedSCOAccessData.h"
#include "PartScrubber.h"
#include "SCOPool.h"
#include "Scrubber.h"
#include "SnapshotManagement.h"
#include "TheSonOfTLogCutter.h"
#include "TLogCutter.h"
#include "TLogMerger.h"
#include "TLogSplitter.h"
#include "TLogReaderUtils.h"

#include <youtils/Assert.h>
#include <youtils/wall_timer.h>

#include <boost/filesystem/fstream.hpp>

#include <backend/BackendInterface.h>
#include <backend/BackendConnectionManager.h>

namespace scrubbing
{

using namespace volumedriver;

const char
Scrubber::scrub_result_string[] = "scrubbing_result";

ScrubberException::ScrubberException(const std::string& what,
                                     const ExitCode exit_code,
                                     const std::string& result)
    : exit_code_(exit_code)
    , result_(result)
{
    what_ += "Exception while running the scrubber: ";
    what_ += what;
}

const char*
ScrubberException::what() const throw()
{
    return what_.c_str();
}

Scrubber::Scrubber(const ScrubberArgs& args,
                   bool verbose)
    : args_(args)
    , backend_interface_(nullptr)
    , verbose_(verbose)
{
    if(args_.fill_ratio > 1.0 or
       args_.fill_ratio < 0.0)
    {
        LOG_ERROR("fill ratio not between acceptable boundaries 0.0 and 1.0 " <<
                  args_.fill_ratio);
        throw fungi::IOException("fill ratio not acceptable");
    }
    minimum_number_of_used_entries = args_.sco_size * args_.fill_ratio;
}

void
Scrubber::SetupBackend()
{
    boost::property_tree::ptree pt;
    args_.backend_config->persist_internal(pt, ReportDefault::T);
    pt.put("version",1);

    auto cm(backend::BackendConnectionManager::create(pt));
    backend_interface_ = cm->newBackendInterface(args_.getNS());
}

bool
Scrubber::isScrubbingResultString(const std::string& name)
{
    uint32_t scrub_result_string_len = sizeof(scrub_result_string) -1;
    return (name.substr(0, scrub_result_string_len) == scrub_result_string)
        and
        UUID::isUUIDString(name.substr(scrub_result_string_len));
}

void
Scrubber::operator()()
{
    // Set up the FilePool
    LOG_INFO("Instantiation the FilePool");
    result_.snapshot_name = args_.snapshot_name;
    FilePool filepool(args_.scratch_dir);

    // Set up the backend
    SetupBackend();
    VERIFY(backend_interface_.get());
    LOG_INFO("getting the sco access data from the backend");
    // Doesn't throw!
    NormalizedSCOAccessData access_data(*backend_interface_);
    youtils::wall_timer metadatascrubtime;

    try
    {
        // Get The Snapshot From the backend and get the tlogs to process

        LOG_INFO("Getting the snapshots file from the backend");

        fs::path snapshot = filepool.newFile("snapshots");
        backend_interface_->read(snapshot,
                                 snapshotFilename(),
                                 InsistOnLatestVersion::T);
        snapshot_persistor_.reset(new SnapshotPersistor(snapshot));
        snapshot_num_ = snapshot_persistor_->getSnapshotNum(args_.snapshot_name);
        snapshot_persistor_->getTLogsInSnapshot(snapshot_num_,
                                                result_.tlog_names_in);
    }
    catch(std::exception& e)
    {
        throw ScrubberException(e.what(),
                                ScrubberException::NoCleanup);
    }
    catch(...)
    {
        throw ScrubberException("Uknown Exception",
                                ScrubberException::NoCleanup);
    }

    boost::this_thread::interruption_point();

    LOG_INFO("Get the tlogs to scrub from the snapshot file");

    VERIFY(not result_.tlog_names_in.empty());

    std::shared_ptr<TLogReaderInterface>
        combined_tlog_reader(makeCombinedTLogReader(filepool.directory(),
                                                    result_.tlog_names_in,
                                                    backend_interface_->clone()));
    // Split the TLogs in parts and go through them
    LOG_INFO("Starting the TLog Splitter");
    scrubbing::ScrubbingSCODataVector scrubbing_data_vector;

    TLogSplitter tlog_splitter(combined_tlog_reader,
                               scrubbing_data_vector,
                               static_cast<RegionExponent>(args_.region_size_exponent),
                               filepool);
    boost::this_thread::interruption_point();
    tlog_splitter();
    boost::this_thread::interruption_point();

    LOG_INFO("Stopped the TLog Splitter");
    if(verbose_)
    {
        LOG_INFO("SCO information after the splitting\n" << scrubbing_data_vector);
    }

    const TLogSplitter::MapType& split_tlog_map = tlog_splitter.getMap();

    LOG_INFO("Metadata scrubbing the region tlogs");

    std::vector<fs::path> tlogs;

    for(TLogSplitter::MapType::const_iterator it = split_tlog_map.begin();
        it != split_tlog_map.end();
        ++it)
    {
        LOG_INFO("Handling tlog for region " << it->first);
        PartScrubber part_scrubber(it,
                                   scrubbing_data_vector,
                                   filepool,
                                   args_.region_size_exponent,
                                   args_.cluster_size_exponent);
        boost::this_thread::interruption_point();
        part_scrubber(tlogs);
        boost::this_thread::interruption_point();
    }

    LOG_INFO("Stopped the region metadatascrubs");
    if(verbose_)
    {
        LOG_INFO("SCO information after the metadatascrub\n" << scrubbing_data_vector);
    }

    // At this point tlogs contains backwards ordered tlogs with the metadatascrubbed scos.
    // We merge them into one forward tlog...

    LOG_INFO("Merging the metadatascrubbed tlogs");

    BackwardTLogMerger backward_merger;
    for(std::vector<fs::path>::const_iterator i = tlogs.begin();
        i != tlogs.end();
        ++i)
    {
        backward_merger.addTLogReader(new BackwardTLogReader(*i));
    }

    const fs::path metadata_scrubbed = filepool.newFile("metadatascrubbed_tlog");
    ClusterLocation last;
    boost::this_thread::interruption_point();
    backward_merger(metadata_scrubbed,
                    &last);
    boost::this_thread::interruption_point();
#ifndef NDEBUG
    uint64_t size = 0;

    for(std::vector<fs::path>::const_iterator i = tlogs.begin();
        i != tlogs.end();
        ++i)
    {
        size += fs::file_size(*i);
    }
    VERIFY(size == fs::file_size(metadata_scrubbed));
#endif
    double metadata_scrubbed_time = metadatascrubtime.elapsed();

    youtils::wall_timer datascrubtime;

    //    No we scrub this
    LOG_INFO("Starting the DataScrub");

    SCOPool scopool(scrubbing_data_vector,
                    metadata_scrubbed,
                    filepool,
                    *backend_interface_,
                    args_.cluster_size_exponent,
                    args_.sco_size,
                    minimum_number_of_used_entries,
                    access_data,
                    last.sco(),
                    result_.new_sconames);

    std::pair<volumedriver::CheckSum, uint64_t> sp_result = scopool();

    // variable 'ss0' set but not used [-Werror=unused-but-set-variable]
    // CheckSum ss0 = sp_result.first;
    uint64_t relocNum = sp_result.second;

    LOG_INFO("Stopped the DataScrub");

    if(verbose_)
    {
        LOG_INFO("SCO information after datascrub\n" << scrubbing_data_vector);

    }

    double data_scrub_time = datascrubtime.elapsed();

    LOG_INFO("Starting the forward merging of tlogs");

    ForwardTLogMerger forward_merger;
    forward_merger.addTLogReader(new TLogReader(scopool.rewritten_tlog_path()));
    forward_merger.addTLogReader(new TLogReader(scopool.nonrewritten_tlog_path()));
    const fs::path result_tlog(filepool.newFile("completely_scrubbed_tlog"));

    // variable 'cs1' set but not used [-Werror=unused-but-set-variable]
    // CheckSum cs1 = forward_merger(result_tlog);
    boost::this_thread::interruption_point();
    forward_merger(result_tlog);
    boost::this_thread::interruption_point();

    LOG_INFO("Finished the forward merging of tlogs");

    LOG_INFO("Starting cutting up the tlog in digestible chuncks");

    // Now we cut up the tlog into pieces again

    TLogCutter t(backend_interface_.get(),
                 result_tlog,
                 filepool);

    boost::this_thread::interruption_point();
    result_.tlogs_out = t();
    boost::this_thread::interruption_point();

    VERIFY(not result_.tlogs_out.empty());

    LOG_INFO("Finished cutting up the tlog in digestible chuncks");

    if(fs::file_size(scopool.relocations_tlog_path()) > 0)
    {
        LOG_INFO("Starting cutting up relocations tlog");

        std::vector<std::string> cutup_relocations;

        TheSonOfTLogCutter t(backend_interface_.get(),
                             scopool.relocations_tlog_path(),
                             filepool,
                             cutup_relocations);
        t();
        VERIFY(not cutup_relocations.empty());
        result_.relocs = cutup_relocations;
        LOG_INFO("Finished cutting up relocations tlog");
    }

    result_.relocNum = relocNum;

    scopool.getSCONamesToBeDeleted(result_.sconames_to_be_deleted);

    scopool.get_access_data(result_.prefetch);

    if(not args_.apply_immediately)
    {
        UUID result_id;
        std::string result_name = std::string(scrub_result_string) + result_id.str();
        VERIFY(isScrubbingResultString(result_name));

        //         No interruption points here
        fs::path result_file_name = filepool.newFile(result_name);
        youtils::Serialization::serializeAndFlush<boost::archive::text_oarchive>(result_file_name,
                                                                                 result_);

        // work around ALBA uploads timing out but eventually succeeding in the
        // background, leading to overwrite on retry.
        TODO("AR: use OverwriteObject::F instead");
        VERIFY(not backend_interface_->objectExists(result_name));
        backend_interface_->write(result_file_name,
                                  result_name,
                                  OverwriteObject::T);
        LOG_INFO("Gotten " << result_.tlog_names_in.size()
                 << " tlogs from the backend and written "
                 << result_.tlogs_out.size() << " back");
        LOG_INFO("Metadata scrub took " << metadata_scrubbed_time << " seconds");
        LOG_INFO("Gotten " << scopool.numberOfSCOSReadFromBackend()
                 << " scos from the backend and written "
                 << scopool.numberOfSCOSWrittenToBackend() << " back");
        LOG_INFO("Data scrub took " << data_scrub_time << " seconds ");
        LOG_INFO("Scrubbing result is in " << result_name);
        scrubbing_result_name = result_name;
    }
    else
    {
        LOG_INFO("Applying the scrubbing result immediately on the backend");
        boost::this_thread::interruption_point();
        snapshot_persistor_->replace(result_.tlog_names_in,
                                     result_.tlogs_out,
                                     snapshot_num_);

        snapshot_persistor_->setSnapshotScrubbed(snapshot_num_,
                                                 true);
        fs::path new_snaps = filepool.newFile("new_snapshots.xml");
        snapshot_persistor_->saveToFile(new_snaps, SyncAndRename::T);
        boost::this_thread::interruption_point();
        backend_interface_->write(new_snaps,
                                  snapshotFilename(),
                                  OverwriteObject::T);
        LOG_INFO("Wrote the new snapshots.xml and its data to the backend, cleaning up");
        boost::this_thread::interruption_point();
        for (const SCO sconame : result_.sconames_to_be_deleted)
        {
            LOG_INFO("Deleting SCO " << sconame.str());
            backend_interface_->remove(sconame.str());
        }
        boost::this_thread::interruption_point();
        for (const auto& tlog_id : result_.tlog_names_in)
        {
            const auto tlog_name(boost::lexical_cast<std::string>(tlog_id));
            LOG_INFO("Deleting TLog " << tlog_name);
            backend_interface_->remove(tlog_name);
        }
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
