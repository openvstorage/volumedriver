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

#include "VolManagerTestSetup.h"

#include <sys/time.h>
#include <sys/resource.h>

#include "../VolManager.h"
#include "../VolumeConfigParameters.h"

namespace volumedriver
{
// Moving this in class breaks the compiler/linker.


template<unsigned num_filedescriptors>
class FileDescriptorResourceLimitTest
    : public VolManagerTestSetup
{

protected:

public:
    FileDescriptorResourceLimitTest()
        : VolManagerTestSetup("FileDescriptorResourceLimitTest")
        , setup(false)
        , rlimit_set(false)
    {
        // dontCatch(); uncomment if you want an unhandled exception to cause a crash, e.g. to get a stacktrace
    }

    virtual void
    SetUp()
    {
        int ret = getrlimit(RLIMIT_NOFILE, &rl_);
        ASSERT_TRUE(ret >= 0);
        {
            struct rlimit rl;
            rl.rlim_cur = num_filedescriptors;
            rl.rlim_max = rl_.rlim_max;
            int ret = setrlimit(RLIMIT_NOFILE, &rl);
            ASSERT_TRUE(ret >= 0);
        }
        rlimit_set = true;
        VolManagerTestSetup::SetUp();
        setup = true;
        auto vm = VolManager::get();
        ASSERT_EQ(vm->getMaxFileDescriptors(), num_filedescriptors);
    }

    virtual void
    TearDown()
    {
        if(setup)
        {
            VolManagerTestSetup::TearDown();
        }
        if(rlimit_set)
        {
            setrlimit(RLIMIT_NOFILE, &rl_);
        }
    }

private:
    uint64_t original_number_of_filedescriptors;
    struct rlimit rl_;
    bool setup, rlimit_set;

};

const static unsigned num_open_files = 256;
typedef FileDescriptorResourceLimitTest<num_open_files> Test1;

TEST_P(Test1, DISABLED_filedescriptors)
{
    const std::string name1("vol1");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    SharedVolumePtr v1 = newVolume(name1,
                           ns1);

    ASSERT_TRUE(v1 != nullptr);

    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& ns2 = ns2_ptr->ns();

    const std::string name2("vol2");
    SharedVolumePtr v2 = newVolume(name2,
                           ns2);

    ASSERT_TRUE(v2 != nullptr);

    writeToVolume(*v2,
                  Lba(0),
                  4096,
                  "foo");

    const SnapshotName snapname("snap");
    v2->createSnapshot(snapname);

    writeToVolume(*v2,
                  Lba(0),
                  4096,
                  "bar");

    waitForThisBackendWrite(*v2);

    VolumeConfig cfg2 = v2->get_config();

    ASSERT_NO_THROW(destroyVolume(v2,
                                  DeleteLocalData::F,
                                  RemoveVolumeCompletely::F));

    v2 = nullptr;

    //    auto vm = VolManager::get();

    // rl.rlim_cur = 3 + // std{in,out,err}
    //     vm->getOpenSCOsPerVolume() * 2 +
    //     vm->globalFDEstimate() +
    //     vm->volumeFDEstimate();
    // ASSERT_NO_THROW(set_nofile_limit(rl));

    auto ns3_ptr = make_random_namespace();

    const backend::Namespace& ns3 = ns3_ptr->ns();

    const std::string name3("vol3");
    //    const backend::Namespace ns3;

    ASSERT_THROW(newVolume(name3, ns3), VolManager::InsufficientResourcesException) <<
        "volume creation must fail if the fd limit is exceeded";

    ASSERT_THROW(localRestart(ns2), VolManager::InsufficientResourcesException) <<
        "local restart must fail if the fd limit is exceeded";

    ASSERT_THROW(restartVolume(cfg2), VolManager::InsufficientResourcesException) <<
        "backend restart must fail if the fd limit is exceeded";

    //    const backend::Namespace ns4;
    auto ns4_ptr = make_random_namespace();

    const backend::Namespace& ns4 = ns4_ptr->ns();

    const std::string name4("clone_of_" + name2);
    ASSERT_THROW(createClone(name4, ns4, ns2, snapname),
                 VolManager::InsufficientResourcesException) <<
        "creating a clone must fail if the fd limit is exceeded";
}

TEST_P(Test1, maxVolumes)
{
    // Y42 this is testing WHAT WE DO NOT SUPPORT
    auto ns_ptr = make_random_namespace();
    // fill_backend_cache(ns_ptr->ns());


    auto vm = VolManager::get();

    const unsigned maxvols =  (num_open_files - (3 + vm->globalFDEstimate())) / vm->volumeFDEstimate();


    //  ASSERT_NO_THROW(set_nofile_limit(rl));
    std::vector<std::unique_ptr<WithRandomNamespace> > nss;

    for (size_t i = 0; i < maxvols; ++i)
    {
        std::stringstream ss;
        ss << "vol" << i;

        nss.push_back(make_random_namespace());

        ASSERT_NO_THROW(newVolume(ss.str(),
                                  nss.back()->ns())) << ss.str() << " failed";
    }

    std::stringstream ss;
    ss << "vol" << maxvols;

    // std::stringstream ns;
    // ns << "openvstorage-volumedrivertest-vol" << maxvols;

    nss.push_back(make_random_namespace());

    ASSERT_THROW(newVolume(ss.str(),
                           nss.back()->ns()),
                 VolManager::InsufficientResourcesException);
}
INSTANTIATE_TEST(Test1);


}
