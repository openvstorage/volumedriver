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

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <youtils/ArakoonInterface.h>
#include <youtils/Catchers.h>
#include <youtils/FileUtils.h>
#include <youtils/FileDescriptor.h>
#include <youtils/wall_timer.h>
#include <youtils/System.h>

#include <volumedriver/Api.h>
#include <volumedriver/Volume.h>

#include <filesystem/ObjectRouter.h>
#include <filesystem/Registry.h>

#include "../PythonClient.h"

namespace volumedriverfstest
{

using namespace volumedriverfs;

namespace ara = arakoon;
namespace bc = boost::chrono;
namespace be = backend;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace vd = volumedriver;
namespace yt = youtils;

#define LOCKVD()                                        \
    fungi::ScopedLock ag__(api::getManagementMutex())

// *** WARNING ***
//
// These tests need to be carefully crafted to make them debuggable with gdb /
// valgrind.
// In particular, the following scenario leads to deadlocks when run under gdb or
// valgrind which typically lead to remote calls timing out and the test failing
// unexpectedly:
//
// * general description of these tests: the main process runs the test and a local
//   FileSystem instance (L) (w/o FUSE!); a child is forked off that runs a full-fledged
//   volumedriverfs (R) (including FUSE)
// * gdb is debugging L
// * an object O (volume or file) is owned by L
// * an operation (stat, read, write, ....) on O is invoked on R which leads to R
//   redirecting it to L
// => The redirected call is then not processed until the operation on R times out.
//
// I suspect that this is due to threads behaving differently when run under gdb?
class RemoteTest
    : public FileSystemTestBase
{
public:
    RemoteTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("RemoteTest")
                             .redirect_timeout_ms(10000)
                             .backend_sync_timeout_ms(9500)
                             .migrate_timeout_ms(500)
                             .redirect_retries(1)
                             .scrub_manager_interval_secs(3600))
        , remote_root_(mount_dir(remote_dir(topdir_)))
    {}

    virtual void
    SetUp()
    {
        FileSystemTestBase::SetUp();
        start_failovercache_for_remote_node();
        mount_remote();
    }

    virtual void
    TearDown()
    {
        umount_remote();
        stop_failovercache_for_remote_node();
        FileSystemTestBase::TearDown();
    }

    void
    check_remote_file(const fs::path& rpath,
                      const std::string& pattern,
                      uint64_t off)
    {
        yt::FileDescriptor sio(rpath, yt::FDMode::Read);
        std::vector<char> rbuf(pattern.size());
        sio.pread(&rbuf[0], rbuf.size(), off);

        const std::string red(&rbuf[0], rbuf.size());
        EXPECT_EQ(pattern, red);
    }

    void
    write_to_remote_file(const fs::path& rpath,
                         const std::string& pattern,
                         uint64_t off,
                         bool sync = false)
    {
        yt::FileDescriptor sio(rpath, yt::FDMode::Write, CreateIfNecessary::T);
        sio.pwrite(pattern.c_str(), pattern.size(), off);
        if (sync)
        {
            sio.sync();
        }
    }

    fs::path
    make_remote_file(const fs::path& fname, const uint64_t size)
    {
        const fs::path rpath(remote_root_ / fname);
        resize_file(rpath, size);
        return rpath;
    }

    void
    resize_file(const fs::path& path, const uint64_t size)
    {
        yt::FileDescriptor sio(path, yt::FDMode::Write, CreateIfNecessary::T);
        sio.truncate(size);
    }

    void
    wait_for_file(const fs::path& fname)
    {
        for (auto i = 0; i < 100; ++i)
        {
            if (fs::exists(fname))
            {
                return;
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }
        }

        FAIL() << fname << " does not exist";
    }

    void
    fake_remote_registration(const ObjectId& id)
    {
        std::shared_ptr<CachedObjectRegistry>
            registry(fs_->object_router().object_registry());
        ObjectRegistrationPtr reg(registry->find(id,
                                                 IgnoreCache::F));
        ASSERT_TRUE(reg != nullptr);

        registry->drop_entry_from_cache(id);

        ASSERT_TRUE(fs_->object_router().node_id() == local_node_id());

        bpt::ptree pt;
        std::shared_ptr<yt::LockedArakoon>
            larakoon(new Registry(make_registry_config_(pt),
                                  RegisterComponent::F));
        OwnerTagAllocator owner_tag_allocator(vrouter_cluster_id(),
                                              larakoon);

        ObjectRegistrationPtr
            new_reg(new ObjectRegistration(reg->getNS(),
                                           id,
                                           remote_node_id(),
                                           reg->treeconfig.object_type == ObjectType::Volume ?
                                           ObjectTreeConfig::makeBase() :
                                           ObjectTreeConfig::makeFile(),
                                           owner_tag_allocator(),
                                           FailOverCacheConfigMode::Automatic));

        registry->TESTONLY_add_to_cache_(new_reg);
    }

    void
    test_stale_registration(const FrontendPath& fname)
    {
        const auto rpath(remote_root_ / fname);

        const uint64_t vsize = 10 << 20;
        const ObjectId oid(create_file(fname, vsize));

        fake_remote_registration(oid);

        const std::string pattern("locally written");
        write_to_file(fname, pattern, pattern.size(), 0);
        check_file(fname, pattern, pattern.size(), 0);
    }

    void
    test_larceny_(const FrontendPath& fname,
                  std::function<void(const FrontendPath&,
                                     const ObjectId&,
                                     const std::string& pattern,
                                     const uint64_t offset)> fun_without_remote)
    {
        const uint64_t vsize = 10 << 20;
        const auto rpath(make_remote_file(fname, vsize));

        const auto maybe_id(find_object(fname));
        ASSERT_TRUE(static_cast<bool>(maybe_id));

        const uint64_t off = 42;
        const std::string pattern("remotely written");

        write_to_remote_file(rpath,
                             pattern,
                             off);

        umount_remote();

        std::shared_ptr<ClusterRegistry> reg(cluster_registry(fs_->object_router()));
        reg->set_node_state(remote_node_id(),
                            ClusterNodeStatus::State::Offline);

        fun_without_remote(fname, *maybe_id, pattern, off);

        verify_registration(*maybe_id,
                            local_node_id());

        // check that there is no outdated cached entry.
        std::shared_ptr<CachedObjectRegistry> oreg(fs_->object_router().object_registry());
        EXPECT_EQ(local_node_id(),
                  oreg->find(*maybe_id,
                             IgnoreCache::F)->node_id);
    }

    void
    test_larceny(const FrontendPath& fname)
    {
        test_larceny_(fname,
                      [&](const FrontendPath& fname,
                          const ObjectId&,
                          const std::string& pattern,
                          const uint64_t offset)
                      {
                          // do a multi-threaded read to get an idea how that
                          // interacts with the remote timeout
                          std::list<std::thread> threads;

                          const uint32_t cpus = ::sysconf(_SC_NPROCESSORS_ONLN);
                          const uint32_t nthreads =
                              std::min<uint32_t>(8,
                                                 std::max<uint32_t>(2,
                                                                    cpus));
                          for (uint32_t i = 0; i < nthreads; ++i)
                          {
                              threads.emplace_back(std::thread([&]()
                                                               {
                                                                   check_file(fname,
                                                                              pattern,
                                                                              pattern.size(),
                                                                              offset);
                                                               }));
                          }

                          for (auto& t : threads)
                          {
                              t.join();
                          }
                      });
    }

    void
    test_theft_while_migrating(const FrontendPath& fname)
    {
        test_larceny_(fname,
                      [&](const FrontendPath& fname,
                          const ObjectId& id,
                          const std::string& pattern,
                          const uint64_t offset)
                      {
                          fs_->object_router().migrate(id);

                          check_file(fname,
                                     pattern,
                                     pattern.size(),
                                     offset);
                      });
    }

    void
    test_migration(const FrontendPath& fname)
    {
        const uint64_t vsize = 10 << 20;
        const auto rpath(make_remote_file(fname, vsize));

        check_stat(fname, vsize);
        wait_for_file(rpath);

        auto maybe_id(find_object(fname));
        ASSERT_TRUE(maybe_id != boost::none);

        verify_registration(*maybe_id, remote_node_id());

        const uint64_t off = 4095;
        const std::string pattern("written on the remote instance");

        write_to_remote_file(rpath, pattern, off);

        fs_->migrate(ObjectId(*maybe_id));
        verify_registration(*maybe_id, local_node_id());

        check_file(fname, pattern, pattern.size(), off);
    }

    void
    test_auto_migration_on_write(const FrontendPath& fname)
    {
        const uint64_t wthresh = 10;
        set_volume_write_threshold(wthresh);
        set_file_write_threshold(wthresh);

        const uint64_t vsize = 10 << 20;
        const auto rpath(make_remote_file(fname, vsize));

        check_stat(fname, vsize);
        wait_for_file(rpath);

        auto maybe_id(find_object(fname));
        ASSERT_TRUE(static_cast<bool>(maybe_id));

        verify_registration(*maybe_id, remote_node_id());

        const uint64_t off = 0;
        const std::string pattern("written on the local instance to the remote volume");

        for (uint64_t i = 0; i < wthresh - 1; ++i)
        {
            write_to_file(fname, pattern.c_str(), pattern.size(), off);
            verify_registration(*maybe_id, remote_node_id());
        }

        write_to_file(fname, pattern.c_str(), pattern.size(), off);

        verify_registration(*maybe_id, local_node_id());

        check_file(fname, pattern, pattern.size(), off);
    }

    void
    test_auto_migration_on_read(const FrontendPath& fname)
    {
        const uint64_t rthresh = 10;
        set_volume_read_threshold(rthresh);
        set_file_read_threshold(rthresh);

        const uint64_t vsize = 10 << 20;
        const auto rpath(make_remote_file(fname, vsize));

        check_stat(fname, vsize);
        wait_for_file(rpath);

        auto maybe_id(find_object(fname));
        ASSERT_TRUE(static_cast<bool>(maybe_id));

        verify_registration(*maybe_id, remote_node_id());

        static const std::vector<uint8_t> ref(4096, 0);

        for (uint64_t i = 0; i < rthresh - 1; ++i)
        {
            std::vector<char> rbuf(ref.size(), 'a');
            const auto r = read_from_file(fname,
                                          &rbuf[0],
                                          rbuf.size(),
                                          i * rbuf.size());

            EXPECT_EQ(static_cast<ssize_t>(rbuf.size()), r);
            EXPECT_EQ(0, memcmp(&rbuf[0], &ref[0], rbuf.size()));

            verify_registration(*maybe_id, remote_node_id());
        }

        std::vector<char> rbuf(ref.size(), 'b');
        const auto r = read_from_file(fname,
                                      &rbuf[0],
                                      rbuf.size(),
                                      (rthresh - 1) * rbuf.size());

        EXPECT_EQ(static_cast<ssize_t>(rbuf.size()), r);
        EXPECT_EQ(0, memcmp(&rbuf[0], &ref[0], rbuf.size()));

        verify_registration(*maybe_id, local_node_id());
    }

    // Might not work reliably under gdb / valgrind - see above.
    void
    dreaded_threaded_threat(const FrontendPath& fname)
    {
        const uint64_t vsize = 10 << 20;
        const ObjectId vname(create_file(fname, vsize));

        std::atomic_bool stop = { false };

        uint8_t nworkers = ::sysconf(_SC_NPROCESSORS_ONLN);
        if (nworkers > 4)
        {
            nworkers = 4;
        }

        ASSERT_LT(0, nworkers);

        std::vector<std::thread> workers;
        workers.reserve(nworkers);

        // We're intentionally reading from a pristine (unwritten) volume to speed up
        // the migration (otherwise the we'd have to wait for written data to hit the backend)
        // as we're only interested in hitting the small time window during which the volume
        // registration is outdated (i.e. after destruction on the migration source but
        // before restart on the migration target).
        for (uint8_t i = 0; i < nworkers; ++i)
        {
            workers.push_back(std::thread([&]
                                          {
                                              std::vector<char> buf(4096);
                                              uint64_t off = 0;
                                              while (not stop)
                                              {
                                                  read_from_file(fname,
                                                                 buf.data(),
                                                                 buf.size(),
                                                                 off);
                                                  off = (off + buf.size()) % vsize;
                                              }
                                          }));
        }

        const unsigned roundtrips = 23;
        std::thread mover([&]
                          {
                              try
                              {
                                  for (unsigned i = 0; i < roundtrips; ++i)
                                  {
                                      client_.migrate(vname.str(), remote_node_id());
                                      client_.migrate(vname.str(), local_node_id());
                                  }
                                  stop = true;
                              }
                              CATCH_STD_ALL_EWHAT({
                                      stop = true;
                                      FAIL() << "mover thread caught exception: " << EWHAT;
                                  });
                          });

        mover.join();

        for (auto& w : workers)
        {
            w.join();
        }
    }

    // Might not work reliably under gdb / valgrind - see above.
    void
    test_remove_while_migrating(const FrontendPath& fname)
    {
        const unsigned iterations = 29;

        for (unsigned i = 0; i < iterations; ++i)
        {
            verify_absence(fname);

            const ObjectId vname(create_file(fname));

            std::thread mover([&]
                              {
                                  try
                                  {
                                      client_.migrate(vname.str(),
                                                      remote_node_id());
                                  }
                                  catch (std::exception&)
                                  {
                                      // it's ok if this one throws. we might want to be
                                      // more specific about the exception though?
                                  }
                              });

            EXPECT_EQ(0, unlink(fname));

            mover.join();
        }
    }

    // Might not work reliably under gdb / valgrind - see above.
    void
    test_migrate_while_removing(const FrontendPath& fname)
    {
        const unsigned iterations = 29;

        for (unsigned i = 0; i < iterations; ++i)
        {
            verify_absence(fname);

            const ObjectId vname(create_file(fname));

            const fs::path rpath(remote_root_ / fname);
            wait_for_file(rpath);

            std::thread mover([&]
                              {
                                  try
                                  {
                                      client_.migrate(vname.str(),
                                                      remote_node_id());
                                  }
                                  catch (std::exception&)
                                  {
                                      // it's ok if this one throws. we might want to be
                                      // more specific about the exception though?
                                  }
                              });

            boost::system::error_code ec;
            fs::remove_all(rpath, ec);
            EXPECT_EQ(0, ec.value()) << "removal of " << rpath << " failed: " << ec;

            mover.join();
        }
    }

    void
    test_create_and_destroy(bool volume)
    {
        const FrontendPath fname(volume ?
                                 make_volume_name("/some-volume") :
                                 "/some-file");
        const auto rpath(remote_root_ / fname);

        EXPECT_FALSE(fs::exists(rpath));

        EXPECT_EQ(rpath, make_remote_file(fname, 0));

        check_stat(fname, 0);
        EXPECT_EQ(0U, fs::file_size(rpath));

        {
            const auto maybe_id(find_object(fname));

            ObjectRegistrationPtr reg(find_registration(*maybe_id));
            EXPECT_EQ(volume ?
                      ObjectType::Volume :
                      ObjectType::File,
                      reg->object().type);

            verify_registration(*maybe_id, remote_node_id());
        }

        const uint64_t size = 10ULL << 20;
        resize_file(rpath, size);

        check_stat(fname, size);
        EXPECT_EQ(size, fs::file_size(rpath));

        fs::remove_all(rpath);
        EXPECT_FALSE(fs::exists(rpath));

        verify_absence(fname);
    }

    void
    test_read_write(bool volume)
    {
        const FrontendPath fname(volume ?
                                 make_volume_name("/some-volume") :
                                 "/some-file");

        const uint64_t size = 10ULL << 20;
        const auto rpath(make_remote_file(fname, size));

        const std::string pattern1("written remotely by a node not owning the volume");
        const uint64_t off = 4095;
        write_to_file(fname, pattern1, pattern1.size(), off);

        check_remote_file(rpath, pattern1, off);

        const std::string pattern2("written locally by the node owning the volume");

        write_to_remote_file(rpath, pattern2, off);
        check_file(fname, pattern2, pattern2.size(), off);
    }

    void
    check_foc_state(const ObjectId& id,
                    const FailOverCacheConfigMode exp_mode,
                    const boost::optional<vd::FailOverCacheConfig>& exp_config,
                    const vd::VolumeFailOverState exp_state)
    {
        EXPECT_EQ(exp_mode,
                  client_.get_failover_cache_config_mode(id.str()));
        EXPECT_EQ(exp_config,
                  client_.get_failover_cache_config(id.str()));
        EXPECT_EQ(exp_state,
                  boost::lexical_cast<vd::VolumeFailOverState>(client_.info_volume(id.str()).failover_mode));
    }

    void
    test_migration_and_manual_foc_config(const boost::optional<vd::FailOverCacheConfig>& manual_cfg,
                                         const vd::VolumeFailOverState state)
    {
        const uint64_t vsize = 1ULL << 20;

        const FrontendPath fname(make_volume_name("/local-volume"));
        const ObjectId id(create_file(fname,
                                      vsize));

        client_.set_manual_failover_cache_config(id.str(),
                                                 manual_cfg);

        check_foc_state(id,
                        FailOverCacheConfigMode::Manual,
                        manual_cfg,
                        state);

        client_.migrate(id.str(),
                        remote_node_id(),
                        true);

        check_foc_state(id,
                        FailOverCacheConfigMode::Manual,
                        manual_cfg,
                        state);
    }

    // Returning the LocalNode allows callers to keep the local voldrv alive while stopping
    // vfsprotocol messaging.
    std::shared_ptr<LocalNode>
    shut_down_object_router()
    {
        std::shared_ptr<LocalNode> local_node(fs_->object_router().local_node_());
        fs_->object_router().shutdown_();
        return local_node;
    }

    const fs::path remote_root_;
};

TEST_F(RemoteTest, ping)
{
    ObjectRouter& vrouter = fs_->object_router();
    EXPECT_NO_THROW(vrouter.ping(remote_node_id()));
    umount_remote();

    EXPECT_THROW(vrouter.ping(remote_node_id()),
                 RequestTimeoutException);

    mount_remote();
    EXPECT_NO_THROW(vrouter.ping(remote_node_id()));
}

TEST_F(RemoteTest, volume_create_and_destroy)
{
    test_create_and_destroy(true);
}

TEST_F(RemoteTest, file_create_and_destroy)
{
    test_create_and_destroy(false);
}

TEST_F(RemoteTest, volume_read_write)
{
    test_read_write(true);
}

TEST_F(RemoteTest, file_read_write)
{
    test_read_write(false);
}

TEST_F(RemoteTest, stale_volume_registration)
{
    test_stale_registration(FrontendPath(make_volume_name("/some-volume")));
}

// Cf. OVS-4498:
// (0) filedriver file F is owned by node N
// (1) F is moved to node M
// (2) node P still has a registration for F that points to N in its cache
// (3) if P sends a request for F to N, N returns an I/O error to P instead of
//     an "ObjectNotRunningHere" (which would result in P dropping the stale entry
//     and retrying after fetching the registration again from arakoon)
TEST_F(RemoteTest, stale_file_registration)
{
    test_stale_registration(FrontendPath("/some-file"));
}

// This was a fun one: the DirectoryEntry was cached to speed up lookups, which of course
// went completely wrong once the remote renamed a file - oy vey. Let's make sure we
// don't run into it again:
TEST_F(RemoteTest, file_rename)
{
    const FrontendPath fname1("/some-file");
    const auto rpath1(make_remote_file(fname1, 0));

    const std::string pattern1("written before rename");
    write_to_file(fname1, pattern1, pattern1.size(), 0);

    check_remote_file(rpath1, pattern1, 0);

    const fs::path fname2("/some-other-file");
    const auto rpath2(make_remote_file(fname2, 0));

    const std::string pattern2("written after rename");

    write_to_remote_file(rpath2, pattern2, 0);
    fs::rename(rpath2, rpath1);

    check_file(fname1, pattern2, pattern2.size(), 0);
}

TEST_F(RemoteTest, volume_migration)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    test_migration(fname);
}

TEST_F(RemoteTest, file_migration)
{
    const FrontendPath fname("/some-file");
    test_migration(fname);
}

TEST_F(RemoteTest, volume_auto_migration_on_write)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    test_auto_migration_on_write(fname);
}

TEST_F(RemoteTest, file_auto_migration_on_write)
{
    const FrontendPath fname("/some-file");
    test_auto_migration_on_write(fname);
}

TEST_F(RemoteTest, volume_auto_migration_on_read)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    test_auto_migration_on_read(fname);
}

TEST_F(RemoteTest, file_auto_migration_on_read)
{
    const FrontendPath fname("/some-file");
    test_auto_migration_on_read(fname);
}

// TODO: tests which simulate node crashes (i.e. rely on the FOC)
TEST_F(RemoteTest, volume_larceny)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    test_larceny(fname);
}

TEST_F(RemoteTest, file_larceny)
{
    const FrontendPath fname("/some-file");
    test_larceny(fname);
}

TEST_F(RemoteTest, volume_theft_while_migrating)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    test_theft_while_migrating(fname);
}

TEST_F(RemoteTest, file_theft_while_migrating)
{
    const FrontendPath fname("/some-file");
    test_theft_while_migrating(fname);
}

TEST_F(RemoteTest, remote_temporarily_out_of_service)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto rpath(make_remote_file(fname, vsize));

    const auto maybe_volume_id(find_object(fname));
    ASSERT_TRUE(static_cast<bool>(maybe_volume_id));

    const uint64_t off = 13;
    const std::string pattern("remotely written");

    write_to_remote_file(rpath,
                         pattern,
                         off);

    Handle::Ptr h;

    EXPECT_EQ(0, open(fname, h, O_RDONLY));

    umount_remote();

    std::vector<char> buf(pattern.size());
    EXPECT_GT(0, read(fname,
                      buf.data(),
                      buf.size(),
                      off,
                      *h));

    mount_remote();

    EXPECT_EQ(static_cast<ssize_t>(pattern.size()),
              read(fname,
                   buf.data(),
                   buf.size(),
                   off,
                   *h));

    EXPECT_EQ(pattern,
              std::string(buf.data(), buf.size()));

    EXPECT_EQ(0,
              release(fname, std::move(h)));

    verify_registration(*maybe_volume_id,
                        remote_node_id());
}

TEST_F(RemoteTest, remote_gone_and_offlined_after_a_while)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto rpath(make_remote_file(fname, vsize));

    const auto maybe_volume_id(find_object(fname));
    ASSERT_TRUE(static_cast<bool>(maybe_volume_id));

    const uint64_t off = 13;
    const std::string pattern("remotely written");

    write_to_remote_file(rpath,
                         pattern,
                         off);

    Handle::Ptr h;

    EXPECT_EQ(0, open(fname, h, O_RDONLY));

    umount_remote();

    {
        const fs::path p(yt::FileUtils::create_temp_file(topdir_,
                                                         "config.json"));
        ALWAYS_CLEANUP_FILE(p);

        api::persistConfiguration(p, false);

        bpt::ptree pt;
        bpt::json_parser::read_json(p.string(), pt);

        const ip::PARAMETER_TYPE(vrouter_redirect_retries)
            val(std::numeric_limits<uint32_t>::max());

        val.persist(pt);
        api::updateConfiguration(pt);
    }

    const uint32_t retries = 3;

    std::thread t([&]()
                  {
                      std::this_thread::sleep_for(std::chrono::milliseconds(redirect_timeout_ms_ * retries));
                      std::shared_ptr<ClusterRegistry>
                          reg(cluster_registry(fs_->object_router()));
                      reg->set_node_state(remote_node_id(),
                                          ClusterNodeStatus::State::Offline);
                  });

    std::vector<char> buf(pattern.size());

    yt::wall_timer w;

    EXPECT_EQ(static_cast<ssize_t>(pattern.size()),
              read(fname,
                   buf.data(),
                   buf.size(),
                   off,
                   *h));

    EXPECT_LT(retries * redirect_timeout_ms_ / 1000, w.elapsed());

    t.join();

    EXPECT_EQ(pattern, std::string(buf.data(), buf.size()));
    EXPECT_EQ(0, release(fname, std::move(h)));

    verify_registration(*maybe_volume_id,
                        local_node_id());
}

TEST_F(RemoteTest, directory_listing)
{
    const std::string parent_name("/directory");
    const FrontendPath parent(parent_name);
    const fs::path rparent(remote_root_ / parent_name);

    EXPECT_FALSE(fs::exists(rparent));
    EXPECT_EQ(0, mkdir(parent, S_IRWXU));

    EXPECT_TRUE(fs::exists(rparent));
    EXPECT_TRUE(fs::is_directory(rparent));

    const std::string
        volume_name((make_volume_name("/volume")).string());
    const FrontendPath volume((fs::path(parent.str()) / volume_name).string());
    const fs::path rvolume(rparent / volume_name);

    EXPECT_FALSE(fs::exists(rvolume));
    EXPECT_EQ(rvolume, make_remote_file(volume, 0));

    EXPECT_TRUE(fs::exists(rvolume)) << rvolume << " does not exist";
    EXPECT_TRUE(fs::is_regular_file(rvolume)) << rvolume << " is not a regular file";

    {
        struct stat st;
        getattr(volume, st);
        EXPECT_TRUE(S_ISREG(st.st_mode));
    }

    const std::string file_name("file");
    const FrontendPath file((fs::path(parent.str()) / file_name).string());
    const fs::path rfile(rparent / file_name);

    EXPECT_FALSE(fs::exists(rfile));
    EXPECT_EQ(rfile, make_remote_file(file, 0));

    EXPECT_TRUE(fs::exists(rfile)) << rfile << " does not exist";
    EXPECT_TRUE(fs::is_regular_file(rfile)) << rfile << " is not a file";

    {
        struct stat st;
        getattr(file, st);
        EXPECT_TRUE(S_ISREG(st.st_mode));
    }

    const std::string child_name("child");
    const FrontendPath child((fs::path(parent.str()) / child_name).string());
    const fs::path rchild(rparent / child_name);

    EXPECT_FALSE(fs::exists(rchild));
    EXPECT_EQ(0, mkdir(child, S_IRWXU));

    EXPECT_TRUE(fs::exists(rchild));
    EXPECT_TRUE(fs::is_directory(rchild));

    {
        struct stat st;
        getattr(child, st);
        EXPECT_TRUE(S_ISDIR(st.st_mode));
    }

    fs::recursive_directory_iterator it(remote_root_);
    fs::recursive_directory_iterator end;

    std::set<fs::path> paths;
    while (it != end)
    {
        paths.insert(*it);
        ++it;
    }

    EXPECT_EQ(1U, paths.erase(rparent)) << rparent << " not found";
    EXPECT_EQ(1U, paths.erase(rvolume)) << rvolume << " not found";
    EXPECT_EQ(1U, paths.erase(rfile)) << rfile << " not found";
    EXPECT_EQ(1U, paths.erase(rchild)) << rchild << " not found";

    EXPECT_TRUE(paths.empty());
}

TEST_F(RemoteTest, locally_list_remote_volumes)
{
    unsigned count = 3;
    std::set<ObjectId> vols;
    const uint64_t vsize = 10 << 20;

    for (unsigned i = 0; i < count; ++i)
    {
        const FrontendPath fname(make_volume_name("/volume-" +
                                                  boost::lexical_cast<std::string>(i)));
        const auto rpath(make_remote_file(fname, vsize));
        auto maybe_id(find_object(fname));
        ASSERT_TRUE(static_cast<bool>(maybe_id));

        vols.insert(*maybe_id);
    }

    EXPECT_EQ(count, vols.size());

    auto voll(fs_->object_router().object_registry()->list());

    EXPECT_EQ(count, voll.size());

    for (const auto& v : voll)
    {
        EXPECT_EQ(1U, vols.erase(v));
    }

    EXPECT_TRUE(vols.empty());
}

TEST_F(RemoteTest, volume_entry_cache)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto rpath(make_remote_file(fname, vsize));

    wait_for_file(rpath);

    EXPECT_TRUE(find_in_volume_entry_cache(fname) == nullptr);

    // any call that looks up the VolumeEntry will drag it into the cache - let's
    // try a getattr()
    check_stat(fname, vsize);
    auto maybe_id(find_object(fname));

    {
        DirectoryEntryPtr entry(find_in_volume_entry_cache(fname));
        ASSERT_TRUE(entry != nullptr);
        EXPECT_EQ(*maybe_id, entry->object_id());
    }

    fs::remove_all(rpath);

    // it's still cached ...
    {
        DirectoryEntryPtr entry(find_in_volume_entry_cache(fname));
        ASSERT_TRUE(entry != nullptr);
        EXPECT_EQ(*maybe_id, entry->object_id());
    }

    // ... we can even open it:
    Handle::Ptr h;

    EXPECT_EQ(0, open(fname, h, O_RDWR));

    {
        DirectoryEntryPtr entry(find_in_volume_entry_cache(fname));
        ASSERT_TRUE(entry != nullptr);
        EXPECT_EQ(*maybe_id, entry->object_id());
    }

    // .. only once we try to operate on it do we get an error and the entry is gone:
    std::vector<char> buf(1);
    EXPECT_EQ(-ENOENT, read(fname, buf.data(), buf.size(), 0, *h));
    EXPECT_TRUE(find_in_volume_entry_cache(fname) == nullptr);

    EXPECT_EQ(0, release(fname, std::move(h)));
}

TEST_F(RemoteTest, volume_dreaded_threaded_threat)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    dreaded_threaded_threat(fname);
}

TEST_F(RemoteTest, file_dreaded_threaded_threat)
{
    const FrontendPath fname("/some-file");
    dreaded_threaded_threat(fname);
}

TEST_F(RemoteTest, volume_remove_while_migrating)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    test_remove_while_migrating(fname);
}

TEST_F(RemoteTest, file_remove_while_migrating)
{
    const FrontendPath fname("/some-file");
    test_remove_while_migrating(fname);
}

TEST_F(RemoteTest, volume_migrate_while_removing)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    test_migrate_while_removing(fname);
}

TEST_F(RemoteTest, file_migrate_while_removing)
{
    const FrontendPath fname("/some-file");
    test_migrate_while_removing(fname);
}

// Disabled and needs to be fixed by OVS-205
TEST_F(RemoteTest, DISABLED_unlink_deleted_but_still_registered_volume)
{
    const FrontendPath fname(make_volume_name("/volume"));
    const ObjectId id(create_file(fname));
    const fs::path rpath(remote_root_ / fname);

    verify_presence(fname);
    EXPECT_TRUE(fs::exists(rpath));

    {
        LOCKVD();
        api::destroyVolume(vd::VolumeId(id.str()),
                           vd::DeleteLocalData::T,
                           vd::RemoveVolumeCompletely::T,
                           vd::DeleteVolumeNamespace::F,
                           vd::ForceVolumeDeletion::T);
    }

    boost::system::error_code ec;
    EXPECT_TRUE(fs::remove(rpath)) << rpath << ": error code " << ec.value() << " (" <<
        ec.message() << ")";
    EXPECT_EQ(0, ec.value());

    EXPECT_FALSE(fs::exists(rpath));

    EXPECT_FALSE(fs::remove(rpath));
    EXPECT_EQ(0, ec.value());
}

TEST_F(RemoteTest, focced)
{
    const uint64_t vsize = 1ULL << 20;

    const FrontendPath local_fname(make_volume_name("/local-volume"));
    const ObjectId local_id(create_file(local_fname, vsize));
    verify_presence(local_fname);

    const FrontendPath remote_fname(make_volume_name("/remote-volume"));
    const fs::path rpath(make_remote_file(remote_fname, vsize));
    verify_presence(remote_fname);

    const auto maybe_remote_id(find_object(remote_fname));
    EXPECT_TRUE(static_cast<bool>(maybe_remote_id));

    const XMLRPCVolumeInfo local_info(client_.info_volume(local_id));
    const XMLRPCVolumeInfo remote_info(client_.info_volume(*maybe_remote_id));

    {
        EXPECT_EQ(remote_config().failovercache_port, local_info.failover_port);
        std::stringstream ss(local_info.failover_mode);
        vd::VolumeFailOverState st;
        ss >> st;
        EXPECT_EQ(vd::VolumeFailOverState::OK_SYNC, st);
    }

    {
        EXPECT_EQ(local_config().failovercache_port, remote_info.failover_port);
        std::stringstream ss(remote_info.failover_mode);
        vd::VolumeFailOverState st;
        ss >> st;
        EXPECT_EQ(vd::VolumeFailOverState::OK_SYNC, st);
    }
}

TEST_F(RemoteTest, online_offline_node_listing)
{
    using State = ClusterNodeStatus::State;

    EXPECT_EQ(State::Online, client_.info_cluster()[remote_node_id()]);
    EXPECT_NO_THROW(client_.mark_node_offline(remote_node_id()));
    EXPECT_EQ(State::Offline, client_.info_cluster()[remote_node_id()]);

    //idempotency
    EXPECT_NO_THROW(client_.mark_node_offline(remote_node_id()));

    EXPECT_NO_THROW(client_.mark_node_online(remote_node_id()));
    EXPECT_EQ(State::Online, client_.info_cluster()[remote_node_id()]);

    //idempotency
    EXPECT_NO_THROW(client_.mark_node_online(remote_node_id()));

    EXPECT_THROW(client_.mark_node_offline(local_node_id()),
                 clienterrors::InvalidOperationException);
    EXPECT_EQ(State::Online, client_.info_cluster()[local_node_id()]);

    EXPECT_NO_THROW(client_.mark_node_online(local_node_id()));
    EXPECT_EQ(State::Online, client_.info_cluster()[local_node_id()]);

    EXPECT_THROW(client_.mark_node_online("none-existing node"),
                 std::exception);
    EXPECT_THROW(client_.mark_node_offline("none-existing node"),
                 std::exception);
}

TEST_F(RemoteTest, inode_allocation)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    const auto rpath(make_remote_file(fname, 0));

    struct stat lst;
    ASSERT_EQ(0, getattr(fname, lst));

    struct stat rst;
    ASSERT_EQ(0, ::stat(rpath.string().c_str(), &rst));

    EXPECT_EQ(lst.st_ino, rst.st_ino);
}

TEST_F(RemoteTest, clone_from_unavailable_template)
{
    const FrontendPath tpath(make_volume_name("/template"));
    const uint64_t size = 10ULL << 20;

    make_remote_file(tpath, size);
    boost::optional<ObjectId> maybe_tname(find_object(tpath));

    ASSERT_TRUE(static_cast<bool>(maybe_tname));
    client_.set_volume_as_template(*maybe_tname);

    umount_remote();

    const FrontendPath cpath("/clone");
    client_.create_clone_from_template(cpath.str(),
                                       make_metadata_backend_config(),
                                       *maybe_tname);
}

TEST_F(RemoteTest, destruction)
{
    bpt::ptree pt;

    {
        LOCKVD();
        api::persistConfiguration(pt, false);
    }

    const auto rfpath(make_remote_file("/file.remote", 1ULL << 20));

    const FrontendPath vname(make_volume_name("/volume.remote"));
    const auto rvpath(make_remote_file(vname, 1ULL << 20));

    const auto maybe_vid(find_object(vname));
    const be::Namespace vnspace(find_registration(*maybe_vid)->getNS());
    const be::Namespace fnspace(PARAMETER_VALUE_FROM_PROPERTY_TREE(fd_namespace, pt));

    umount_remote();
    stop_fs();

    auto cm(be::BackendConnectionManager::create(pt, RegisterComponent::F));
    auto fbi(cm->newBackendInterface(fnspace));
    auto vbi(cm->newBackendInterface(vnspace));

    ASSERT_TRUE(fbi->namespaceExists());
    ASSERT_TRUE(vbi->namespaceExists());

    const std::string pfx("");

    std::shared_ptr<yt::LockedArakoon> reg(new Registry(pt, RegisterComponent::F));
    EXPECT_LT(0U, reg->prefix(pfx).size());

    FileSystem::destroy(pt);

    ClusterRegistry(vrouter_cluster_id(),
                    reg).erase_node_configs();

    EXPECT_FALSE(fbi->namespaceExists());
    EXPECT_FALSE(vbi->namespaceExists());

    const ara::value_list l(reg->prefix(pfx));
    EXPECT_EQ(0U, l.size());

    auto it = l.begin();
    ara::arakoon_buffer kbuf;

    while (it.next(kbuf))
    {
        const std::string k(static_cast<const char*>(kbuf.second),
                            kbuf.first);
        std::cout << "arakoon: leaked key " << k << std::endl;
    }
}

TEST_F(RemoteTest, remove_remote_clone)
{
    const FrontendPath ppath(make_volume_name("/parent"));
    const uint64_t size = 10ULL << 20;

    const ObjectId pname(create_file(ppath, size));

    const std::string pattern("before");
    write_to_file(ppath, pattern, 1024 * pattern.size(), 0);

    const std::string snap(client_.create_snapshot(pname));

    wait_for_snapshot(pname,
                      snap);

    const FrontendPath cpath("/clone");
    const ObjectId cname(client_.create_clone(cpath.str(),
                                              make_metadata_backend_config(),
                                              pname,
                                              snap,
                                              remote_node_id()));

    // const fs::path rppath(remote_root_ / ppath);

    ASSERT_NE(0, unlink(ppath));
    // ASSERT_THROW(fs::remove_all(rppath),
    //              std::exception);

    // ASSERT_TRUE(fs::exists(rppath));

    const fs::path rcpath(remote_root_ / clone_path_to_volume_path(cpath));
    ASSERT_TRUE(fs::exists(rcpath));

    fs::remove_all(rcpath);
    ASSERT_FALSE(fs::exists(rcpath));

    ASSERT_EQ(0, unlink(ppath));
    // fs::remove_all(rppath);
    // ASSERT_FALSE(fs::exists(rppath));
}

TEST_F(RemoteTest, volume_migrate_timeout)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto rpath(make_remote_file(fname,
                                      vsize));

    const uint64_t rthresh = 1;
    set_volume_read_threshold(rthresh);
    set_volume_write_threshold(0);
    set_backend_sync_timeout(boost::chrono::milliseconds(1));

    auto maybe_id(find_object(fname));
    ASSERT_TRUE(static_cast<bool>(maybe_id));

    verify_registration(*maybe_id,
                        remote_node_id());

    const uint64_t off = 13;
    const std::string pattern("remotely written");

    for (size_t i = 0; i < 100; ++i)
    {
        write_to_remote_file(rpath,
                             pattern,
                             off);
    }

    EXPECT_THROW(fs_->object_router().migrate(*maybe_id),
                 RemoteTimeoutException);

    boost::this_thread::sleep_for(boost::chrono::milliseconds(migrate_timeout_ms_ + 1));

    verify_registration(*maybe_id,
                        remote_node_id());

    set_backend_sync_timeout(boost::chrono::milliseconds(9500));

    for (size_t i = 0; i < rthresh + 1; ++i)
    {
        std::vector<char> buf(pattern.size());
        const auto r = read_from_file(fname,
                                      buf.data(),
                                      buf.size(),
                                      off);
        EXPECT_EQ(static_cast<ssize_t>(buf.size()),
                  r);

        const std::string s(buf.data(),
                            buf.size());

        EXPECT_EQ(pattern,
                  s);
    }

    verify_registration(*maybe_id,
                        local_node_id());
}

TEST_F(RemoteTest, owner_tag_after_migration)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t size = 10ULL << 20;

    make_remote_file(fname,
                     size);

    boost::optional<ObjectId> maybe_id(find_object(fname));
    const ObjectRegistrationPtr old_reg(find_registration(*maybe_id));
    fs_->migrate(*maybe_id);

    const ObjectRegistrationPtr new_reg(find_registration(*maybe_id));
    EXPECT_NE(old_reg->owner_tag,
              new_reg->owner_tag);

    LOCKVD();

    const vd::VolumeConfig
        cfg(api::getVolumeConfig(static_cast<vd::VolumeId>(*maybe_id)));
    ASSERT_EQ(new_reg->owner_tag,
              cfg.owner_tag_);
}

TEST_F(RemoteTest, location_based_caching)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t size = 10ULL << 20;

    const ObjectId vname(create_file(fname,
                                     size));
    const vd::VolumeId volid(vname.str());

    {
        LOCKVD();
        api::setClusterCacheBehaviour(volid,
                                      vd::ClusterCacheBehaviour::CacheOnWrite);
        api::setClusterCacheMode(volid,
                                 vd::ClusterCacheMode::LocationBased);
    }

    const std::string pattern1("first");
    write_to_file(fname,
                  pattern1.data(),
                  pattern1.size(),
                  0);

    check_file(fname,
               pattern1,
               pattern1.size(),
               0);

    client_.migrate(vname.str(),
                    remote_node_id());

    check_file(fname,
               pattern1,
               pattern1.size(),
               0);

    const std::string pattern2("second");
    write_to_file(fname,
                  pattern2.data(),
                  pattern2.size(),
                  0);

    client_.migrate(vname.str(),
                    local_node_id());

    check_file(fname,
               pattern2,
               pattern2.size(),
               0);
}

TEST_F(RemoteTest, auto_migration_without_foc)
{
    const uint64_t rthresh = 10;
    set_volume_read_threshold(rthresh);

    const uint64_t vsize = 10ULL << 20;
    const FrontendPath fname(make_volume_name("/some-volume"));

    const auto rpath(make_remote_file(fname, vsize));
    const auto maybe_id(find_object(fname));

    check_stat(fname, vsize);
    wait_for_file(rpath);

    const std::vector<char> ref(4096, 'X');

    write_to_remote_file(rpath,
                         std::string(ref.begin(),
                                     ref.end()),
                         0,
                         true);

    stop_failovercache_for_remote_node();

    for (uint64_t i = 0; i < rthresh + 1; ++i)
    {
        std::vector<char> rbuf(ref.size(), 'a');
        const auto r = read_from_file(fname,
                                      rbuf.data(),
                                      rbuf.size(),
                                      0);

        EXPECT_EQ(static_cast<ssize_t>(rbuf.size()), r);
        EXPECT_EQ(0, memcmp(rbuf.data(),
                            ref.data(),
                            ref.size()));
    }

    verify_registration(*maybe_id, local_node_id());
}

TEST_F(RemoteTest, resize)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    const fs::path rpath(make_remote_file(fname,
                                          0));

    const size_t size = 1ULL << 20;

    EXPECT_EQ(0,
              truncate(fname,
                       size));

    check_stat(fname,
               size);
}

TEST_F(RemoteTest, fsync)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    const fs::path rpath(make_remote_file(fname,
                                          1ULL << 20));


    Handle::Ptr h;
    EXPECT_EQ(0,
              open(fname,
                   h,
                   O_WRONLY));

    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         EXPECT_EQ(0, release(fname,
                                                              std::move(h)));
                                     }));

    const std::string pattern("not that interesting really");
    EXPECT_EQ(pattern.size(),
              write(*h,
                    pattern.data(),
                    pattern.size(),
                    0));

    EXPECT_EQ(0,
              fsync(*h,
                    false));
}

// https://github.com/openvstorage/volumedriver/issues/67
// fuse invokes FuseInterface::releasedir with path set to nullptr
TEST_F(RemoteTest, unlink_open_directory)
{
    const fs::path rdir(remote_root_ / "directory");
    fs::create_directories(rdir);
    ASSERT_TRUE(fs::exists(rdir));

    const int fd = ::open(rdir.string().c_str(),
                          O_RDONLY);
    ASSERT_LE(0, fd);

    auto on_exit(yt::make_scope_exit([fd]
                                     {
                                         ::close(fd);
                                     }));

    fs::remove_all(rdir);
}

TEST_F(RemoteTest, only_steal_from_offlined_node)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    const size_t vsize = 1ULL << 20;

    const auto rpath(make_remote_file(fname,
                                      vsize));

    const off_t off = 0;
    const std::string pattern("remotely written");
    write_to_remote_file(rpath,
                         pattern,
                         off);

    umount_remote();

    std::shared_ptr<ClusterRegistry> reg(cluster_registry(fs_->object_router()));
    EXPECT_EQ(ClusterNodeStatus::State::Online,
              reg->get_node_state(remote_node_id()));

    std::vector<char> buf(pattern.size());
    EXPECT_GT(0,
              read_from_file(fname,
                             buf.data(),
                             buf.size(),
                             off));
    EXPECT_EQ(ClusterNodeStatus::State::Online,
              reg->get_node_state(remote_node_id()));
}

TEST_F(RemoteTest, stealing_and_fencing)
{
    umount_remote();
    set_use_fencing(true);
    mount_remote();

    const FrontendPath vpath(make_volume_name("/some-volume"));
    const uint64_t vsize = 10ULL << 20;
    const ObjectId oid(create_file(vpath,
                                   vsize));

    const std::string pattern1("written first");

    write_to_file(vpath,
                  pattern1.data(),
                  pattern1.size(),
                  vsize / 2);

    const std::string snap(client_.create_snapshot(oid));
    wait_for_snapshot(oid,
                      snap);

    // cling to the LocalNode so we can still access the local volumedriver afterwards
    std::shared_ptr<LocalNode> local_node(shut_down_object_router());

    check_remote_file(remote_root_ / vpath,
                      pattern1,
                      vsize / 2);

    const vd::VolumeId vid(oid.str());

    LOCKVD();
    // triggers a volume halt by trying to write to the backend.
    EXPECT_THROW(api::setFailOverCacheConfig(vid,
                                             boost::none),
                 std::exception);
    EXPECT_TRUE(api::getHalted(vid));
}

// Cf. https://github.com/openvstorage/volumedriver/issues/145
TEST_F(RemoteTest, back_to_life)
{
    const FrontendPath vname(make_volume_name("/some-volume"));
    const size_t vsize = 1ULL << 20;
    const fs::path rpath(make_remote_file(vname, vsize));

    const std::string pattern("remotely written");
    const size_t off = 0;

    write_to_remote_file(rpath,
                         pattern,
                         off);

    set_redirect_timeout(bc::milliseconds(500));
    umount_remote();

    std::vector<char> buf(pattern.size());

    std::shared_ptr<RemoteNode> rnode(remote_node(fs_->object_router(),
                                                  remote_node_id()));
    ASSERT_TRUE(rnode != nullptr);

    const size_t extra_reads = yt::System::get_env_with_default("REMOTE_TEST_BACK_TO_LIFE_EXTRA_READS",
                                                                50ULL);

    auto fun([&]
             {
                 while (not remote_node_queue_full(*rnode))
                 {
                     EXPECT_GT(0,
                               read_from_file(vname,
                                              buf.data(),
                                              buf.size(),
                                              off));
                 }

                 for (size_t i = 0; i < extra_reads; ++i)
                 {
                     EXPECT_GT(0,
                               read_from_file(vname,
                                              buf.data(),
                                              buf.size(),
                                              off));
                 }
             });

    const size_t nthreads = 4;
    std::vector<boost::thread> threads;
    threads.reserve(nthreads);

    for (size_t i = 0; i < nthreads; ++i)
    {
        threads.emplace_back(fun);
    }

    for (auto& t : threads)
    {
        t.join();
    }

    mount_remote();

    threads.clear();

    for (size_t i = 0; i < nthreads; ++i)
    {
        threads.emplace_back([&]
                             {
                                 EXPECT_EQ(buf.size(),
                                           read_from_file(vname,
                                                          buf.data(),
                                                          buf.size(),
                                                          off));
                             });
    }

    for (auto& t : threads)
    {
        t.join();
    }
}

TEST_F(RemoteTest, DISABLED_setup_remote_hack)
{
    sleep(1000000);
}

}
