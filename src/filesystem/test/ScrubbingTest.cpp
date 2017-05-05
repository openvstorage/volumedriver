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

#include "FileSystemTestBase.h"

#include <boost/chrono.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

#include <youtils/FileUtils.h>

#include <backend/BackendConfig.h>
#include <backend/BackendInterface.h>

#include <volumedriver/Api.h>
#include <volumedriver/Scrubber.h>
#include <volumedriver/ScrubberAdapter.h>
#include <volumedriver/ScrubReply.h>
#include <volumedriver/ScrubWork.h>

namespace volumedriverfstest
{

using namespace volumedriverfs;
using namespace std::literals::string_literals;

namespace bc = boost::chrono;
namespace be = backend;
namespace fs = boost::filesystem;
namespace vd = volumedriver;
namespace yt = youtils;

class ScrubbingTest
    : public FileSystemTestBase
{
protected:
    ScrubbingTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("ScrubbingTest")
                             .scrub_manager_interval_secs(1)
                             .use_cluster_cache(false))
    {}

    std::tuple<ObjectId, FrontendPath>
    create_volume()
    {
        const FrontendPath path(make_volume_name("/some-volume"));
        const ObjectId id(create_file(path));
        const size_t vsize = 2 * max_clusters_ * get_cluster_size(id);

        EXPECT_EQ(0,
                  truncate(id, vsize));

        return std::make_tuple(id, path);
    }

    static std::string
    make_pattern(const vd::ClusterAddress ca)
    {
        return "cluster-"s + boost::lexical_cast<std::string>(ca);
    }

    void
    write_data(const ObjectId& id)
    {
        const size_t csize = get_cluster_size(id);
        const std::string maxstr(make_pattern(max_clusters_));

        for (size_t i = 0; i < max_clusters_; ++i)
        {
            const auto s(make_pattern(i));

            EXPECT_EQ(s.size(),
                      write_to_file(id,
                                    s,
                                    s.size(),
                                    i * csize));

            EXPECT_EQ(maxstr.size(),
                      write_to_file(id,
                                    maxstr,
                                    maxstr.size(),
                                    max_clusters_ * csize));
        }
    }

    void
    check_data(const ObjectId& id)
    {
        const size_t csize = get_cluster_size(id);

        for (size_t i = 0; i < max_clusters_; ++i)
        {
            const auto s(make_pattern(i));
            check_file(id,
                       s,
                       s.size(),
                       i * csize);
        }

        const std::string s(make_pattern(max_clusters_));
        check_file(id,
                   s,
                   s.size(),
                   max_clusters_ * csize);
    }

    std::vector<scrubbing::ScrubReply>
    scrub(const ObjectId& id,
          const boost::optional<vd::SnapshotName>& start_snap = boost::none,
          const boost::optional<vd::SnapshotName>& end_snap = boost::none)
    {
        using namespace scrubbing;

        std::vector<ScrubWork>
            works(fs_->object_router().get_scrub_work_local(id,
                                                            start_snap,
                                                            end_snap));

        std::vector<ScrubReply> reps;
        reps.reserve(works.size());

        const yt::Uri uri(configuration_.string());

        for (const auto& w : works)
        {
            const fs::path p(yt::FileUtils::temp_path("scrubber"));
            ALWAYS_CLEANUP_DIRECTORY(p);

            reps.push_back(ScrubberAdapter::scrub(be::BackendConfig::makeBackendConfig(uri),
                                                  w,
                                                  p.string()));
        }

        return reps;
    }

    scrubbing::ScrubberResult
    fetch_scrub_result(const scrubbing::ScrubReply& reply)
    {
        using namespace scrubbing;

        be::BackendInterfacePtr bi(api::backend_connection_manager()->newBackendInterface(reply.ns_));
        ScrubberResult scrub_result;

        bi->fillObject<decltype(scrub_result),
                       ScrubberResult::IArchive>(scrub_result,
                                                 reply.scrub_result_name_,
                                                 InsistOnLatestVersion::T);

        return scrub_result;
    }

    void
    wait_for_scrub_manager(const bc::seconds timeout = bc::seconds(60))
    {
        ScrubManager& sm = local_node(fs_->object_router())->scrub_manager();

        using Clock = bc::steady_clock;
        const Clock::time_point end(Clock::now() + timeout);

        while (Clock::now() <= end)
        {
            if (sm.get_parent_scrubs().empty() and
                sm.get_clone_scrubs().empty() and
                sm.per_node_garbage_().empty())
            {
                return;
            }

            boost::this_thread::sleep_for(bc::milliseconds(100));
        }

        FAIL() << "timeout waiting for scrub manager to finish";
    }

    void
    check_backend_before_gc(const be::Namespace& nspace,
                            const scrubbing::ScrubberResult& res)
    {
        be::BackendInterfacePtr bi(api::backend_connection_manager()->newBackendInterface(nspace));

        for (auto& tlog_id : res.tlog_names_in)
        {
            EXPECT_TRUE(bi->objectExists(boost::lexical_cast<std::string>(tlog_id))) << tlog_id;
        }

        for (auto& tlog : res.tlogs_out)
        {
            EXPECT_TRUE(bi->objectExists(tlog.getName())) << tlog.getName();
        }

        for (auto& sco : res.sconames_to_be_deleted)
        {
            EXPECT_TRUE(bi->objectExists(boost::lexical_cast<std::string>(sco)));
        }
    }

    void
    check_backend_after_gc(const scrubbing::ScrubReply& reply,
                           const scrubbing::ScrubberResult& res)
    {
        be::BackendInterfacePtr bi(api::backend_connection_manager()->newBackendInterface(reply.ns_));

        EXPECT_FALSE(bi->objectExists(reply.scrub_result_name_));

        for (auto& tlog_id : res.tlog_names_in)
        {
            EXPECT_FALSE(bi->objectExists(boost::lexical_cast<std::string>(tlog_id))) << tlog_id;
        }

        for (auto& tlog : res.tlogs_out)
        {
            EXPECT_TRUE(bi->objectExists(tlog.getName())) << tlog.getName();
        }

        for (auto& sco : res.sconames_to_be_deleted)
        {
            EXPECT_FALSE(bi->objectExists(boost::lexical_cast<std::string>(sco)));
        }
    }

    const size_t max_clusters_ = 1024;
};

TEST_F(ScrubbingTest, simple)
{
    ObjectId id;
    std::tie(id, std::ignore) = create_volume();

    write_data(id);

    const vd::SnapshotName snap("snap");
    fs_->object_router().create_snapshot(id,
                                         snap,
                                         0);

    const std::vector<scrubbing::ScrubReply> reps(scrub(id));
    ASSERT_EQ(1, reps.size());
    ASSERT_EQ(id.str(),
              reps[0].ns_.str());
    ASSERT_EQ(snap,
              reps[0].snapshot_name_);

    const scrubbing::ScrubberResult res(fetch_scrub_result(reps[0]));
    ASSERT_EQ(snap,
              res.snapshot_name);
    ASSERT_LT(0,
              res.relocNum);
    ASSERT_FALSE(res.relocs.empty());
    ASSERT_FALSE(res.tlog_names_in.empty());
    ASSERT_FALSE(res.tlogs_out.empty());
    ASSERT_FALSE(res.sconames_to_be_deleted.empty());

    check_backend_before_gc(reps[0].ns_,
                            res);

    fs_->object_router().queue_scrub_reply_local(id,
                                                 reps[0]);

    wait_for_scrub_manager();

    check_data(id);

    check_backend_after_gc(reps[0],
                           res);
}

TEST_F(ScrubbingTest, clones)
{
    ObjectId pid;
    std::tie(pid, std::ignore) = create_volume();

    write_data(pid);

    const vd::SnapshotName snap("snap");
    fs_->object_router().create_snapshot(pid,
                                         snap,
                                         0);

    const std::vector<scrubbing::ScrubReply> reps(scrub(pid));
    ASSERT_EQ(1, reps.size());

    const scrubbing::ScrubberResult res(fetch_scrub_result(reps[0]));
    ASSERT_EQ(snap,
              res.snapshot_name);
    ASSERT_LT(0,
              res.relocNum);
    ASSERT_FALSE(res.relocs.empty());

    const size_t nclones = 2;
    std::vector<ObjectId> clones;
    clones.reserve(nclones);

    for (size_t i = 0; i < nclones; ++i)
    {
        const FrontendPath cpath(make_volume_name("/clone-" +
                                                  boost::lexical_cast<std::string>(i)));
        clones.push_back(ObjectId(fs_->create_clone(cpath,
                                                    make_metadata_backend_config(),
                                                    vd::VolumeId(pid.str()),
                                                    snap).str()));
    }

    check_backend_before_gc(reps[0].ns_,
                            res);

    fs_->object_router().queue_scrub_reply_local(pid,
                                                 reps[0]);

    wait_for_scrub_manager();

    check_data(pid);

    for (auto& cid : clones)
    {
        check_data(cid);
    }

    check_backend_after_gc(reps[0],
                           res);
}

}
