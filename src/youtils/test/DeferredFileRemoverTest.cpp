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

#include "../DeferredFileRemover.h"
#include "../FileDescriptor.h"
#include "../FileUtils.h"
#include "../System.h"
#include <gtest/gtest.h>
#include "../Timer.h"
#include "../UUID.h"

#include <boost/lexical_cast.hpp>

namespace youtilstest
{

using namespace youtils;
namespace fs = boost::filesystem;

class DeferredFileRemoverTest
    : public testing::Test
{
public:
    DeferredFileRemoverTest()
        : root_(FileUtils::temp_path("DeferredFileRemoverTest"))
    {}

    virtual void
    SetUp()
    {
        fs::remove_all(root_);
        fs::create_directories(root_);
    }

    virtual void
    TearDown()
    {
        fs::remove_all(root_);
    }

    std::tuple<std::vector<fs::path>, size_t>
    fill()
    {
        const size_t num_files = System::get_env_with_default("DEFERRED_FILE_REMOVER_TEST_NUM_FILES",
                                                              100ULL);
        const size_t file_size = System::get_env_with_default("DEFERRED_FILE_REMOVER_TEST_FILE_SIZE",
                                                              1ULL << 20);

        return std::make_pair(fill(num_files,
                                   file_size),
                              file_size);
    }

    std::vector<fs::path>
    fill(const size_t num_files,
         const size_t file_size)
    {
        const std::vector<uint8_t> wbuf(file_size, 'q');
        std::vector<fs::path> paths;
        paths.reserve(num_files);

        for (size_t i = 0; i < num_files; ++i)
        {
            paths.emplace_back(root_ / boost::lexical_cast<std::string>(i));
            FileDescriptor fd(paths.back(),
                              FDMode::Write,
                              CreateIfNecessary::T);
            EXPECT_EQ(wbuf.size(),
                      fd.write(wbuf.data(),
                               wbuf.size()));
        }

        return paths;
    }

    void
    wait(DeferredFileRemover& remover)
    {
        remover.wait_for_completion_();
    }

    using RemoveFun = std::function<void(const std::vector<fs::path>&)>;

    void
    perf(RemoveFun fun)
    {
        std::vector<fs::path> paths;
        size_t file_size = 0;

        std::tie(paths, file_size) = fill();

        SteadyTimer t;
        fun(paths);

        auto elapsed = t.elapsed();
        std::cout << "removing " << paths.size() <<
            " files of " << file_size <<
            " bytes took " << elapsed << std::endl;
    }

protected:
    const fs::path root_;
};

TEST_F(DeferredFileRemoverTest, basics)
{
    const std::vector<fs::path> paths(fill(1, 1ULL << 10));
    ASSERT_EQ(1, paths.size());

    const fs::directory_iterator end;

    bool found = false;
    for (fs::directory_iterator it(root_); it != end; ++it)
    {
        ASSERT_EQ(paths.front(),
                  it->path());
        found = true;
    }

    ASSERT_TRUE(found);

    const fs::path garbage_path(root_ / "garbage");
    {
        DeferredFileRemover remover(garbage_path);
        remover.schedule_removal(paths.front());
        wait(remover);
    }

    std::vector<fs::path> paths2;
    for (fs::directory_iterator it(root_); it != end; ++it)
    {
        paths2.emplace_back(it->path());
    }

    ASSERT_EQ(1,
              paths2.size());
    ASSERT_EQ(garbage_path,
              paths2.front());

    ASSERT_TRUE(fs::is_directory(garbage_path));
    ASSERT_TRUE(fs::is_empty(garbage_path));
}

TEST_F(DeferredFileRemoverTest, restart)
{
    const fs::path p(root_ / "garbage");
    fs::create_directories(p);

    for (size_t i = 0; i < 100; ++i)
    {
        FileUtils::touch(p / boost::lexical_cast<std::string>(i));
    }

    ASSERT_FALSE(fs::is_empty(p));

    DeferredFileRemover remover(p);
    wait(remover);

    ASSERT_TRUE(fs::is_empty(p));
}

TEST_F(DeferredFileRemoverTest, direct_removal_performance)
{
    perf([&](const std::vector<fs::path>& paths)
         {
             for (const auto& p : paths)
             {
                 fs::remove(p);
             }
         });
}

TEST_F(DeferredFileRemoverTest, remover_removal_performance)
{
    // keep in mind that this includes the setup cost
    perf([&](const std::vector<fs::path>&)
         {
             DeferredFileRemover r(root_);
             wait(r);
         });
}

TEST_F(DeferredFileRemoverTest, deferred_removal_performance)
{
    DeferredFileRemover remover(root_ / "garbage");

    perf([&](const std::vector<fs::path>& paths)
         {
             for (const auto& p : paths)
             {
                 remover.schedule_removal(p);
             }
         });
}

}
