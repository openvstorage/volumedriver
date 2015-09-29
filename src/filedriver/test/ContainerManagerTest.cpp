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

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <youtils/Catchers.h>
#include <youtils/TestBase.h>
#include <youtils/InitializedParam.h>

#include <backend/BackendTestSetup.h>

#include <filedriver/ContainerManager.h>
#include <filedriver/FileDriverParameters.h>

namespace filedrivertest
{

namespace be = backend;
namespace bpt = boost::property_tree;
namespace fd = filedriver;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace yt = youtils;

class ContainerManagerTest
    : public youtilstest::TestBase
    , public be::BackendTestSetup
{
protected:
    ContainerManagerTest()
        : youtilstest::TestBase()
        , be::BackendTestSetup()
        , topdir_(yt::FileUtils::temp_path () / "ContainerManagerTest")
        , cachedir_(topdir_ / "containercache")
    {}

    virtual void
    SetUp()
    {
        fs::remove_all(topdir_);
        fs::create_directories(topdir_);
        fs::create_directories(cachedir_);
        initialize_connection_manager();


        nspace_ptr_ = make_random_namespace();
        // createTestNamespace(nspace_);
    }

    const be::Namespace&
    nspace() const
    {
        return nspace_ptr_->ns();
    }


    virtual void
    TearDown()
    {
        try
        {
            nspace_ptr_.reset();
            // deleteTestNamespace(nspace_);
        }
        CATCH_STD_ALL_LOG_IGNORE("Failed to delete test namespace ");
        uninitialize_connection_manager();
        fs::remove_all(topdir_);
    }

    be::BackendConnectionManagerPtr
    connection_manager()
    {
        return cm_;
    }

    be::BackendInterfacePtr
    backend_interface(const be::Namespace& ns = be::Namespace())
    {
        return cm_->newBackendInterface(ns);
    }

    bpt::ptree
    make_config(const be::Namespace& ns = be::Namespace()) const
    {
        bpt::ptree pt;
        return make_config(pt,
                           ns);
    }

    bpt::ptree&
    make_config(bpt::ptree& pt,
                const be::Namespace& ns = be::Namespace()) const
    {
        ip::PARAMETER_TYPE(fd_cache_path)(cachedir_.string()).persist(pt);
        ip::PARAMETER_TYPE(fd_namespace)(ns.str()).persist(pt);

        return pt;
    }

    DECLARE_LOGGER("ContainerManagerTest");

    const fs::path topdir_;
    const fs::path cachedir_;
    std::unique_ptr<WithRandomNamespace> nspace_ptr_;

    //    const be::Namespace& nspace_;
};

TEST_F(ContainerManagerTest, construction)
{
    fd::ContainerManager mgr(connection_manager(),
                             make_config());
}

TEST_F(ContainerManagerTest, destruction)
{
    auto cm(connection_manager());
    const be::Namespace ns;
    const bpt::ptree pt(make_config(ns));

    {
        fd::ContainerManager mgr(connection_manager(),
                                 pt);
    }

    auto bi(backend_interface(ns));
    ASSERT_TRUE(bi->namespaceExists());

    fd::ContainerManager::destroy(cm, pt);
    EXPECT_FALSE(bi->namespaceExists());
}

TEST_F(ContainerManagerTest, inexistent_cache_dir)
{
    fs::remove_all(cachedir_);
    EXPECT_THROW(fd::ContainerManager(connection_manager(),
                                      make_config()),
                 std::exception);
}

TEST_F(ContainerManagerTest, cachedir_not_a_directory)
{
    fs::remove_all(cachedir_);
    EXPECT_FALSE(fs::exists(cachedir_));
    {
        fs::ofstream ofs(cachedir_);
        ofs << "this is supposed to be a directory but we made it a file instead to see the ContainerManager failing" << std::endl;
    }

    EXPECT_TRUE(fs::exists(cachedir_));
    EXPECT_THROW(fd::ContainerManager(connection_manager(),
                                      make_config()),
                 std::exception);
}

TEST_F(ContainerManagerTest, restart_with_polluted_cache_dir)
{
    const fs::path p(cachedir_ / "garbage_file");
    {
        fs::ofstream ofs(p);
        ofs << "not of any importance" << std::endl;
    }

    EXPECT_TRUE(fs::exists(p));

    const fs::path q(cachedir_ / "garbage_dir");
    fs::create_directories(q);

    fd::ContainerManager mgr(connection_manager(),
                             make_config());

    EXPECT_FALSE(fs::exists(p));
    EXPECT_FALSE(fs::exists(q));
}

TEST_F(ContainerManagerTest, zero_sized)
{
    const fd::ContainerId cid("empty");

    {
        fd::ContainerManager mgr(connection_manager(),
                                 make_config());

        EXPECT_THROW(mgr.size(cid),
                     std::exception);

        mgr.create(cid);

        EXPECT_EQ(0,
                  mgr.size(cid));
    }

    fd::ContainerManager mgr(connection_manager(),
                             make_config());

    EXPECT_THROW(mgr.size(cid),
                 std::exception);

    mgr.restart(cid);

    EXPECT_EQ(0,
              mgr.size(cid));
}

TEST_F(ContainerManagerTest, read_and_write_and_read)
{
    fd::ContainerManager mgr(connection_manager(),
                             make_config());

    const fd::ContainerId cid("some-container");

    std::vector<char> rbuf(4096);
    EXPECT_THROW(mgr.read(cid, 0, rbuf.data(), rbuf.size()),
                 std::exception);

    mgr.create(cid);

    EXPECT_EQ(0, mgr.read(cid, 0, rbuf.data(), rbuf.size()));

    const std::vector<char> wbuf(rbuf.size(), 'q');
    const off_t off = 3;

    EXPECT_EQ(wbuf.size(),
              mgr.write(cid, off, wbuf.data(), wbuf.size()));

    EXPECT_EQ(wbuf.size() + off, mgr.size(cid));

    ASSERT_LE(off, rbuf.size());
    EXPECT_EQ(off, mgr.read(cid, 0, rbuf.data(), off));

    EXPECT_TRUE(memcmp(std::vector<char>(rbuf.size(), 0).data(),
                       rbuf.data(),
                       rbuf.size()) == 0);

    EXPECT_EQ(rbuf.size(), mgr.read(cid, off, rbuf.data(), rbuf.size()));
    EXPECT_TRUE(memcmp(rbuf.data(), wbuf.data(), rbuf.size()) == 0);
}

TEST_F(ContainerManagerTest, write_and_read_across_extents)
{
    fd::ContainerManager mgr(connection_manager(),
                             make_config());

    const fd::ContainerId cid("some-container");
    mgr.create(cid);

    const size_t wsize = fd::Extent::capacity();
    const off_t off = wsize / 2;

    EXPECT_EQ(0, mgr.size(cid));

    const std::vector<char> wbuf1(wsize / 2, 'X');
    const std::vector<char> wbuf2(wsize / 2, 'Y');

    std::vector<char> wbuf(wsize);
    memcpy(wbuf.data(), wbuf1.data(), wbuf2.size());
    memcpy(wbuf.data() + wbuf1.size(), wbuf2.data(), wbuf2.size());

    EXPECT_EQ(wsize, mgr.write(cid, off, wbuf.data(), wbuf.size()));
    EXPECT_EQ(off + wsize, mgr.size(cid));

    std::vector<char> zbuf(off, 0);
    std::vector<char> rbuf(zbuf.size(), 'Z');

    EXPECT_EQ(rbuf.size(), mgr.read(cid, 0, rbuf.data(), rbuf.size()));

    EXPECT_EQ(0, memcmp(zbuf.data(), rbuf.data(), zbuf.size()));

    EXPECT_EQ(rbuf.size(), mgr.read(cid, rbuf.size(), rbuf.data(), rbuf.size()));
    EXPECT_EQ(0, memcmp(wbuf1.data(), rbuf.data(), rbuf.size()));

    EXPECT_EQ(rbuf.size(), mgr.read(cid, 2 * rbuf.size(), rbuf.data(), rbuf.size()));
    EXPECT_EQ(0, memcmp(wbuf2.data(), rbuf.data(), rbuf.size()));

    EXPECT_EQ(0, mgr.read(cid, off + wsize, rbuf.data(), rbuf.size()));
}

TEST_F(ContainerManagerTest, restart_container)
{
    const fd::ContainerId cid("some-container");
    const std::string pattern("some data");
    const uint32_t off = fd::Extent::capacity() - 3;

    {
        fd::ContainerManager mgr(connection_manager(),
                                 make_config());
        mgr.create(cid);
        EXPECT_EQ(pattern.size(), mgr.write(cid, off, pattern.data(), pattern.size()));

        std::vector<char> rbuf(pattern.size());
        EXPECT_EQ(pattern.size(), mgr.read(cid, off, rbuf.data(), rbuf.size()));
        EXPECT_EQ(pattern, std::string(pattern.data(), pattern.size()));
    }

    fd::ContainerManager mgr(connection_manager(),
                             make_config());

    std::vector<char> rbuf(pattern.size());
    EXPECT_THROW(mgr.read(cid, off, rbuf.data(), rbuf.size()),
                 std::exception);

    mgr.restart(cid);
    EXPECT_EQ(pattern.size(), mgr.read(cid, off, rbuf.data(), rbuf.size()));
    EXPECT_EQ(pattern, std::string(pattern.data(), pattern.size()));
}

TEST_F(ContainerManagerTest, resize_container)
{
    fd::ContainerManager mgr(connection_manager(),
                             make_config());
    const fd::ContainerId cid("some-container");

    auto check([&](uint32_t size)
               {
                   mgr.resize(cid, size);

                   const std::vector<char> zbuf(size, 0);
                   std::vector<char> rbuf(size + 1, 'Q');

                   EXPECT_EQ(size, mgr.size(cid));
                   EXPECT_EQ(size, mgr.read(cid, 0, rbuf.data(), rbuf.size()));
                   EXPECT_EQ(0, memcmp(rbuf.data(), zbuf.data(), zbuf.size()));
               });

    mgr.create(cid);
    EXPECT_EQ(0, mgr.size(cid));

    check(fd::Extent::capacity());
    check(fd::Extent::capacity() + 1);
    check(fd::Extent::capacity() - 1);
    check(1);
    check(0);
}

}
