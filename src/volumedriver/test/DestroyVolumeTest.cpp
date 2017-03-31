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

#include "MDSTestSetup.h"
#include "VolManagerTestSetup.h"

#include "../metadata-server/ClientNG.h"

#include "../Api.h"
#include "../MetaDataBackendConfig.h"

#include <youtils/Assert.h>

namespace volumedrivertest
{
using namespace volumedriver;

namespace fs = boost::filesystem;
namespace mds = metadata_server;

class DestroyVolumeTest
    : public VolManagerTestSetup
{
public:
    DestroyVolumeTest()
        : VolManagerTestSetup(VolManagerTestSetupParameters("DestroyVolumeTest")
                              .use_fawlty_md_stores(UseFawltyMDStores::T)
                              .use_fawlty_tlog_stores(UseFawltyTLogStores::T))
    {}

    void
    check_presence(const VolumeConfig& cfg,
                   DeleteLocalData delete_local,
                   RemoveVolumeCompletely delete_global)
    {
        const fs::path md_path = VolManager::get()->getMetaDataDBFilePath(cfg);
        const fs::path tlog_path = VolManager::get()->getTLogPath(cfg);
        const fs::path snapshots_file_path = VolManager::get()->getSnapshotsPath(cfg);

        TODO("AR: push this out to MetaDataStoreTestSetup!?");
        switch (metadata_backend_type())
        {
        case MetaDataBackendType::Arakoon:
            FAIL() << "fix this test for Arakoon";
            break;
        case MetaDataBackendType::MDS:
            {
                auto client(mds::ClientNG::create(mds_server_config_->node_config));
                const auto nspaces(client->list_namespaces());
                if (delete_global == RemoveVolumeCompletely::F)
                {
                    ASSERT_EQ(1U, nspaces.size());
                    ASSERT_EQ(nspaces.front(),
                              cfg.getNS().str());
                }
                else
                {
                    ASSERT_TRUE(nspaces.empty());
                }
                break;
            }
        case MetaDataBackendType::RocksDB:
        case MetaDataBackendType::TCBT:
            if (delete_local == DeleteLocalData::T)
            {
                EXPECT_FALSE(fs::exists(md_path));
            }
            else
            {
                EXPECT_TRUE(fs::exists(md_path));
            }
            break;
        }

        if (delete_local == DeleteLocalData::T)
        {
            EXPECT_FALSE(fs::exists(tlog_path));
            EXPECT_FALSE(fs::exists(snapshots_file_path));
        }
        else
        {
            EXPECT_TRUE(fs::exists(tlog_path));
            EXPECT_TRUE(fs::exists(snapshots_file_path));
        }

        EXPECT_TRUE(VolManager::get()->createBackendInterface(cfg.getNS())->namespaceExists());
    }

    void
    test_destroy(DeleteLocalData delete_local,
                 RemoveVolumeCompletely delete_global)
    {
        const std::string id1("id");

        auto ns_ptr = make_random_namespace();

        SharedVolumePtr v = VolManagerTestSetup::newVolume(id1,
                                                   ns_ptr->ns());

        writeToVolume(*v,
                      Lba(0),
                      4096,
                      "bad");

        waitForThisBackendWrite(*v);

        const VolumeConfig cfg(v->get_config());

        check_presence(cfg,
                       DeleteLocalData::F,
                       RemoveVolumeCompletely::F);

        try
        {
            fungi::ScopedLock l(api::getManagementMutex());
            api::destroyVolume(v,
                               delete_local,
                               delete_global,
                               DeleteVolumeNamespace::F,
                               ForceVolumeDeletion::F);
        }
        CATCH_STD_ALL_EWHAT({
                (void) EWHAT;
                check_presence(cfg,
                               DeleteLocalData::F,
                               RemoveVolumeCompletely::F);
                throw;
            });

        check_presence(cfg,
                       delete_local,
                       delete_global);
    }
};

TEST_P(DestroyVolumeTest, test_neither_local_nor_global_removal)
{
    test_destroy(DeleteLocalData::F,
                 RemoveVolumeCompletely::F);
}

TEST_P(DestroyVolumeTest, test_only_local_removal)
{
    test_destroy(DeleteLocalData::T,
                 RemoveVolumeCompletely::F);
}

TEST_P(DestroyVolumeTest, test_local_and_global_removal)
{
    test_destroy(DeleteLocalData::T,
                 RemoveVolumeCompletely::T);
}

TEST_P(DestroyVolumeTest, test_only_global_removal)
{
    ASSERT_THROW(test_destroy(DeleteLocalData::F,
                              RemoveVolumeCompletely::T),
                 std::exception);
}


TEST_P(DestroyVolumeTest, test_errors_when_deleting_mdstore)
{
    switch (metadata_backend_type())
    {
    case MetaDataBackendType::Arakoon:
    case MetaDataBackendType::MDS:
    case MetaDataBackendType::RocksDB:
        TODO("AR: revisit - this should also work with RocksDB.");
        return;
    case MetaDataBackendType::TCBT:
        break;
    }

    const std::string id1("id");

    auto ns_ptr = make_random_namespace();
    const backend::Namespace& ns = ns_ptr->ns();

    //    backend::Namespace ns;

    SharedVolumePtr v1 = VolManagerTestSetup::newVolume(id1,
                                                ns);

    writeToVolume(*v1,
                  Lba(0),
                  4096,
                  "bad");

    waitForThisBackendWrite(*v1);

    const fs::path md_path = VolManager::get()->getMetaDataDBFilePath(*v1);
    const fs::path tlog_path = VolManager::get()->getTLogPath(*v1);
    const fs::path snapshots_file_path = VolManager::get()->getSnapshotsPath(*v1);

    const std::string path_regex("/" + ns.str() + "/mdstore.tc");

    add_failure_rule(mdstores_,
                     1,
                     path_regex,
                     {fawltyfs::FileSystemCall::Unlink},
                     0,
                     10,
                     5);

    ASSERT_THROW(destroyVolume(v1,
                               DeleteLocalData::T,
                               RemoveVolumeCompletely::T),
                 boost::filesystem::filesystem_error);

    remove_failure_rule(mdstores_,
                        1);
}

TEST_P(DestroyVolumeTest, test_errors_when_deleting_tlogs)
{
    const std::string id1("id");

    //backend::Namespace ns;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns = ns_ptr->ns();

    SharedVolumePtr v1 = VolManagerTestSetup::newVolume(id1,
                                                ns);

    writeToVolume(*v1,
                  Lba(0),
                  4096,
                  "bad");

    waitForThisBackendWrite(*v1);

    const std::string path_regex("/" + ns.str() + "/tlog_.*");

    add_failure_rule(tlogs_,
                     1,
                     path_regex,
                     {fawltyfs::FileSystemCall::Unlink},
                     0,
                     10,
                     5);

    // ASSERT_THROW(destroyVolume(v1,
    //                            DeleteLocalData::T,
    //                            RemoveVolumeCompletely::T),
    //              boost::filesystem::filesystem_error);

    // remove_failure_rule(mdstores_,
    //                     1);

    // ASSERT_NO_THROW(destroyVolume(v1,
    //                               DeleteLocalData::T,
    //                               RemoveVolumeCompletely::T));

}
INSTANTIATE_TEST(DestroyVolumeTest);

}
