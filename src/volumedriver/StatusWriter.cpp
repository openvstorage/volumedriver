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

#include "StatusWriter.h"

namespace volumedriver_backup
{

namespace be = backend;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace yt = youtils;
namespace vd = volumedriver;

#define WITH_LOCK  fungi::ScopedSpinLock l(spin_lock)

StatusWriter::StatusWriter(const boost::posix_time::time_duration& report_interval,
                           const be::Namespace& target_namespace)
    : report_interval_(report_interval.total_seconds())
    , target_namespace_(target_namespace)
    , report_times(false)
    , seen_(0)
    , kept_(0)
    , total_size_(0)
    , volume_(0)
{
    start_date();
    put("status", "running");
    end_snapshot("");
    start_snapshot("");
    put("total_size", total_size_);

    put("report_interval_in_seconds", report_interval_);
    if(clock_gettime(CLOCK_MONOTONIC,
                     &start_time_) == 0)
    {
        report_times = true;
    }
    else
    {
        LOG_ERROR("Could not get start time, times will not be reported, " <<  strerror(errno));
        put("runtime_in_seconds", "unavailable");
        report_times = false;
    }
}

StatusWriter::~StatusWriter()
{
    if(std::uncaught_exception())
    {
        if(action_)
        {
            action_.reset(0);
            LOG_ERROR("Action was still active, but there is an exception pending");
        }
    }

    VERIFY(not action_);
}


// Call start if a backup has to do real work (total_size > 0)
void
StatusWriter::start(const vd::WriteOnlyVolume* volume,
                    const uint64_t total_size)
{
    LOG_INFO("Writing first status report");
    total_size_ = total_size;
    put("total_size", total_size_);

    volume_ = volume;
    VERIFY(volume_);
    VERIFY(volume_->performance_counters().backend_write_request_size.sum() == 0);


    write();
    VERIFY(not action_);
    LOG_INFO("Starting the action");

    action_.reset(new yt::PeriodicAction("statuswriter",
                                         [this]
                                         {
                                             write();
                                         },
                                         report_interval_));
}

void
StatusWriter::end_snapshot(const std::string& end_snapshot)
{
    WITH_LOCK;
    put("end_snapshot_name", end_snapshot);
}

void
StatusWriter::start_snapshot(const std::string& start_snapshot)
{
    WITH_LOCK;
    put("start_snapshot", start_snapshot);
}

void
StatusWriter::add_seen(const uint64_t size)
{
    WITH_LOCK;
    seen_ += size;
}

void
StatusWriter::add_kept(const uint64_t size)
{
    WITH_LOCK;
    kept_ += size;
}

void
StatusWriter::current_tlog(const std::string& tlog)
{
    WITH_LOCK;
    put("current_tlog", tlog);
}

// Call finish to stop the action and write a last report to the backend
void
StatusWriter::finish()
{
    LOG_INFO("stopping backup reporting");
    action_.reset();

    LOG_INFO("stopped backup reporting");
    if(total_size_ != seen_)
    {
        LOG_WARN("Total size was not equal to seen at the end of the backup");

    }

    put("status", "finished");
    write();
}

uint64_t
StatusWriter::report_interval() const
{
    return report_interval_;
}


void
StatusWriter::start_date()
{
    WITH_LOCK;
    put("start_date", Time::getCurrentTimeAsString());
}

void
StatusWriter::write()
{
    LOG_TRACE("Writing the status to the target backend " << target_namespace_);

    static const std::string backup_json_name("BackupInfo.json");
    std::stringstream out;
    {
        WITH_LOCK;

        put("update_date", Time::getCurrentTimeAsString());
        if(report_times)
        {

            struct timespec write_time;
            if(clock_gettime(CLOCK_MONOTONIC, &write_time) == 0)
            {
                put("runtime_in_seconds", write_time.tv_sec - start_time_.tv_sec);
            }
            else
            {
                LOG_ERROR("Could not get runtine " << strerror(errno));
                put("runtime_in_seconds", "unavailable");
            }
        }
        put("seen", seen_);
        put("kept", kept_);

        uint64_t sent_to_backend =
            volume_ ?
            volume_->performance_counters().backend_write_request_size.sum() :
            0;

        put("sent_to_backend", sent_to_backend);
        // Here we are a little careful and protect against screwed up counters.

        uint64_t still_to_be_examined = (total_size_ >= seen_) ? (total_size_ - seen_) : 0;
        put("still_to_be_examined", still_to_be_examined);
        if(total_size_ < seen_)
        {
            LOG_WARN("Total size (" << total_size_ <<
                     ") <  seen (" << seen_ << "), probably problem with volume counters");

        }

        uint64_t queued = kept_ >= sent_to_backend ? kept_ - sent_to_backend : 0;
        if(kept_ < sent_to_backend)
        {
            LOG_WARN("kept (" << kept_
                     << ") < sent_to_backend (" << sent_to_backend << "), probably problem with volume countres");

        }

        uint64_t pending = still_to_be_examined + queued;

        put("pending", pending);
        bpt::json_parser::write_json(out,
                                     *static_cast<bpt::ptree*>(this));
        LOG_INFO("JSON TO BE SENT IS " << out.str());
        VERIFY(kept_ <= seen_);
    }

    static be::BackendInterfacePtr
        bi_(vd::VolManager::get()->createBackendInterface(target_namespace_));

    try
    {
        const fs::path
            p(yt::FileUtils::create_temp_file_in_temp_dir(target_namespace_.str() + "-" +
                                                          backup_json_name));
        ALWAYS_CLEANUP_FILE(p);
        {

            fs::ofstream ofs(p);
            ofs << out.str();
        }

        bi_->write(p,
                   backup_json_name,
                   OverwriteObject::T);
    }
    catch(std::exception& e)
    {
        LOG_WARN("Could not write " << backup_json_name << " to backend : "
                 << e.what() << ", backup will continue");
    }
    catch(...)
    {
        LOG_INFO("Could not write " << backup_json_name <<
                 " to backend : unknown exception, backup will continue" );
    }

}

#undef WITH_LOCK
}


// Local Variables: **
// mode: c++ **
// End: *
