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

class ResourceLimitTest
    : public VolManagerTestSetup
{
public:
    ResourceLimitTest()
        : VolManagerTestSetup("ResourceLimitTest")
    {
        // dontCatch(); uncomment if you want an unhandled exception to cause a crash, e.g. to get a stacktrace
    }
};


TEST_P(ResourceLimitTest, cacheSize)
{
    //setup assumptions in test:
    //sc_mp1_size = "1GiB", = 1024MiB
    //sc_mp2_size = "1GiB", = 1024MiB
    //sc_trigger_gap = "250MiB",
    //sc_backoff_gap = "500MiB",
    //scos_in_tlog = 20
    // --> available cache space w/o throttling = 1548MiB
    //     in clusters : 396288
    //     given 20 scosintlog-> 19814.4 clusters overall all scoMults is max

    const std::vector<SCOMultiplier> scoMult = { SCOMultiplier(10000),
                                                 SCOMultiplier(5000),
                                                 SCOMultiplier(4000),
                                                 SCOMultiplier(814) };
    VolumeSize volSize(1 << 30);
    std::vector<std::unique_ptr<WithRandomNamespace> > nss;
    for (size_t i = 0; i < scoMult.size(); ++i)
    {
        std::stringstream ss;
        ss << "vol" << i;
        // std::stringstream ns;
        // ns << "openvstorage-volumedrivertest-vol" << i;
        nss.push_back(make_random_namespace());
        ASSERT_NO_THROW(newVolume(ss.str(),
                                  nss.back()->ns(),
                                  volSize,
                                  scoMult[i]))
                << ss.str() << " failed";
    }

    nss.push_back(make_random_namespace());
    ASSERT_THROW(newVolume("shouldFail",
                           nss.back()->ns(),
                           volSize,
                           SCOMultiplier(1)),
                 VolManager::InsufficientResourcesException);
}

TEST_P(ResourceLimitTest, scrutinize_metadata_freespace_when_cloning_or_restarting)
{
    if (metadata_backend_type() == MetaDataBackendType::Arakoon)
    {
        return;
    }

    const std::string vname("volume");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& ns1 = ns_ptr->ns();

    //    const backend::Namespace ns1;

    SharedVolumePtr v = newVolume(VolumeId(vname),
                          ns1);
    ASSERT_TRUE(v != nullptr);

    writeToVolume(*v, 0, 65536, vname);
    const SnapshotName snap("snap");
    v->createSnapshot(snap);
    waitForThisBackendWrite(*v);

    const uint64_t fri =
        FileUtils::filesystem_free_size(VolManager::get()->getMetaDataPath());

    // A rather conservative way to make really sure there is not enough metadata
    // space: 24 bytes per 4k -> a volsize of >= fri * clustersize / 24 is used.
    // Once TCBTMetaDataStore::locally_required_bytes() provides better
    // estimates a more fine grained check would be nice.
    const VolumeId fake("fake");
    // const Namespace fakens;
    auto ns1_ptr = make_random_namespace();

    const backend::Namespace& fakens = ns1_ptr->ns();

    VolumeSize fakesize(fri *
                        default_cluster_size() *
                        24);
    if ((fakesize % default_cluster_size()) != 0)
    {
        fakesize = (fakesize / default_cluster_size() + 1) * default_cluster_size();
    }

    const VolumeConfig fakecfg(VanillaVolumeConfigParameters(fake,
                                                             fakens,
                                                             fakesize,
                                                             OwnerTag(1))
                               .cluster_multiplier(default_cluster_multiplier()));

    setNamespaceRestarting(fakens, fakecfg);

    const std::string cname("clone");
    //const Namespace cns;
    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& cns = ns2_ptr->ns();

    ASSERT_THROW(createClone(cname, cns, ns1, snap),
                 VolManager::InsufficientResourcesException);

    clearNamespaceRestarting(fakens);

    SharedVolumePtr c = createClone(cname, cns, ns1, snap);
    ASSERT_TRUE(c != nullptr);

    const VolumeConfig clonecfg(c->get_config());

    destroyVolume(c,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    setNamespaceRestarting(fakens, fakecfg);
    ASSERT_THROW(localRestart(cns),
                 VolManager::InsufficientResourcesException);

    clearNamespaceRestarting(fakens);
    ASSERT_NO_THROW(c = localRestart(cns));
    ASSERT_TRUE(c != nullptr);

    destroyVolume(c,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    setNamespaceRestarting(fakens, fakecfg);
    ASSERT_THROW(restartVolume(clonecfg),
                VolManager::InsufficientResourcesException);

    clearNamespaceRestarting(fakens);
    restartVolume(clonecfg);

    c = getVolume(VolumeId(cname));
    ASSERT_TRUE(c != nullptr);
}

INSTANTIATE_TEST(ResourceLimitTest);

}

// Local Variables: **
// mode: c++ **
// End: **
