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

#include <vector>

#include "boost/filesystem.hpp"
#include "boost/foreach.hpp"

#include "ExGTest.h"
#include "backend/BackendInterface.h"
#include "../TransientException.h"
#include "../VolManager.h"
#include "../SCOCache.h"
#include "VolManagerTestSetup.h"
#include "../VolumeDriverError.h"

#include "../DataStoreNG.h"

namespace volumedriver
{

namespace fs = boost::filesystem;

class DataStoreNGTest
    : public VolManagerTestSetup
{
protected:
    DataStoreNGTest()
        : VolManagerTestSetup("DataStoreNGTest",
                              UseFawltyMDStores::F,
                              UseFawltyTLogStores::F,
                              UseFawltyDataStores::F,
                              4,
                              "10MiB",
                              "10MiB",
                              "4MiB",
                              "5MiB")
        , vol_(0)
        , dStore_(0)
    {
    }

    virtual void SetUp()
    {
        VolManagerTestSetup::SetUp();
        ns_ptr_ = make_random_namespace();
        vol_ = newVolume("volume1",
                         getNamespace());
        ASSERT(vol_ != nullptr);
        dStore_ = vol_->getDataStore();
        ASSERT(dStore_ != nullptr);
    }

    const backend::Namespace&
    getNamespace()
    {
        return ns_ptr_->ns();
    }

    virtual void TearDown()
    {
        if(vol_ != nullptr)
        {
            destroyVolume(vol_,
                          DeleteLocalData::T,
                          RemoveVolumeCompletely::T);
        }
        VolManagerTestSetup::TearDown();
    }

    void
    writeClusters(size_t n,
                  uint32_t pattern,
                  ClusterLocation *loc,
                  int retries = 7)
    {
	const uint64_t bufsz = dStore_->getClusterSize() * n;
	std::vector<uint8_t> buf(bufsz);
	uint64_t off;
	uint32_t *p;

	for (p = (uint32_t *) &buf[0], off = 0; off < bufsz;
             off += sizeof(*p), ++p)
            *p = pattern;

        size_t wsize = 0;
        for (size_t i = 0; i < n; i += wsize)
        {
            wsize = std::min(dStore_->getRemainingSCOCapacity(),
                             static_cast<ssize_t>(n - i));
            ASSERT_LT(0U,
                      wsize);

            int tries = retries;
            while (true)
            {
                try
                {
                    uint32_t dummy1;
                    std::vector<ClusterLocation> locs(wsize);
                    std::unique_ptr<CheckSum> cs;

                    dStore_->writeClusters(&buf[i],
                                           locs,
                                           locs.size(),
                                           dummy1);

                    ASSERT_LE(0,
                              dStore_->getRemainingSCOCapacity());

                    if (dStore_->getRemainingSCOCapacity() == 0)
                    {
                        dStore_->finalizeCurrentSCO();
                    }

                    for (size_t j = 0; j < locs.size(); ++j)
                    {
                        *(loc + i + j) = locs[j];
                    }
                }
                catch (TransientException &)
                {
                    LOG_WARN("backend congestion detected, retrying");
                    if (--tries < 0)
                        throw;
                    sleep(1);
                    continue;
                }
                break;
            }
        }
    }

    void
    readCluster(uint8_t* buf,
                const ClusterLocation& loc,
                BackendInterfacePtr bi)
    {
        std::vector<ClusterReadDescriptor> descs;
        ClusterLocationAndHash loc_and_hash(loc,
                                            buf);
        descs.push_back(ClusterReadDescriptor(loc_and_hash,
                                              0,
                                              buf,
                                              std::move(bi)));
        dStore_->readClusters(descs);
    }

    void
    readClusters(int n,
                 uint32_t pattern,
                 const ClusterLocation *loc,
                 bool verify = true,
                 int retries = 7)
    {
        uint64_t bufsz = dStore_->getClusterSize() * n;
        std::vector<uint8_t> buf(bufsz);
        //        uint64_t off;

        std::vector<ClusterReadDescriptor> descs;
        descs.reserve(n);

        for (int i = 0; i < n; ++i)
        {
            ClusterLocationAndHash loc_and_hash(loc[i],
                                                &buf[0] + (i * dStore_->getClusterSize()));
            BackendInterfacePtr bi = vol_->getBackendInterface(loc[i].cloneID())->clone();
            VERIFY(bi);
            descs.push_back(ClusterReadDescriptor(loc_and_hash,
                                                  i * dStore_->getClusterSize(),
                                                  &buf[i * dStore_->getClusterSize()],
                                                  std::move(bi)));
        }

        while (true)
        {
            try
            {
                dStore_->readClusters(descs);
            }
            catch (TransientException &)
            {
                LOG_WARN("backend congestion detected, retrying");
                if (--retries < 0)
                    throw;
                sleep(1);
                continue;
            }
            break;
        }

        if (verify) {
            for (uint32_t* p = (uint32_t *) buf.data(), off = 0; off < bufsz;
                 off += sizeof(*p), ++p)
            {
                ASSERT_EQ(pattern, *p) << "p: " << p <<
                    " off: " << off;
            }
        }
    }

    void
    stopVolume()
    {
        VolManager::stop();
        vol_ = 0;
        dStore_ = 0;
    }

    void
    startVolume()
    {
        ASSERT(not vol_);
        startVolManager();
        vol_ = newVolume("volume1",
			 getNamespace());
        ASSERT(vol_);
        dStore_ = vol_->getDataStore();
        ASSERT(dStore_);

    }

    void
    truncate_sco_in_cache(SCO sconame,
                          uint64_t size,
                          bool must_exist = true)
    {
        CachedSCOPtr sco =
            VolManager::get()->getSCOCache()->findSCO(getNamespace(),
                                                      sconame);
        if (must_exist)
        {
            ASSERT_TRUE(sco != nullptr);
        }

        if (sco)
        {
            ASSERT_TRUE(fs::exists(sco->path()));
            FileUtils::truncate(sco->path(),
                                size);
        }
    }

    void
    truncate_sco_in_backend(SCO sco, uint64_t size)
    {
        BackendInterfacePtr bi = vol_->getBackendInterface()->clone();
        ASSERT_TRUE(bi != 0);

        fs::path tmp_file;
        ASSERT_NO_THROW(tmp_file = getTempPath(sco.str()));
        ALWAYS_CLEANUP_FILE(tmp_file);

        ASSERT_NO_THROW(bi->read(tmp_file, sco.str(), InsistOnLatestVersion::T));

        ASSERT_TRUE(fs::exists(tmp_file));
        FileUtils::truncate(tmp_file, size);

        ASSERT_NO_THROW(bi->write(tmp_file,
                                  sco.str(),
                                  OverwriteObject::T));

        EXPECT_EQ(size, bi->getSize(sco.str()));
    }

    //typedef DataStoreNG::SCOCheckSumStore DataStoreCheckSumStore;

    // DataStoreCheckSumStore&
    // getCheckSumStore()
    // {
    //     return dStore_->checkSumStore_;
    // }

    DECLARE_LOGGER("DataStoreNGTest");

    Volume* vol_;
    std::unique_ptr<WithRandomNamespace> ns_ptr_;

    DataStoreNG *dStore_;
};

TEST_P(DataStoreNGTest, DISABLED_invalidParams)
{
    // DataStoreConfig defaultConfig;
    // defaultConfig.clusterSize = 4096;
    // defaultConfig.path = datastore_;
    // defaultConfig.defaultSCOSize = volConfig_.sco_mult;
    // defaultConfig.size = volConfig_.ds.file.cache_sz;
    // Y42 fix when appropriate
    // // passing vol_ to these 2 tests is ok since the volume will not be used
    // VolumeConfig invalidConfig = volConfig_;

    // invalidConfig.cluster_mult_ = 0;

    // EXPECT_THROW(new DataStoreNG(invalidConfig,
    //                              VolManager::get()->getSCOCache()),
    //              fungi::IOException);


    // invalidConfig = volConfig_;
    // invalidConfig.lba_sz = 0;
    // EXPECT_THROW(new DataStoreNG(invalidConfig,
    //                              VolManager::get()->getSCOCache()),
    //              fungi::IOException);

    // invalidConfig = volConfig_;
    // invalidConfig.sco_mult = 0;
    // EXPECT_THROW(new DataStoreNG(invalidConfig,
    //                              VolManager::get()->getSCOCache()),
    //              fungi::IOException);
}

TEST_P(DataStoreNGTest, overflowCache)
{
    int n = (sc_mp1_size_ + sc_mp2_size_) / dStore_->getClusterSize();
    std::vector<ClusterLocation> loc(n);
    EXPECT_NO_THROW(writeClusters(n, 0x90ABCDEF, &loc[0]));
    EXPECT_NO_THROW(dStore_->sync());
    EXPECT_NO_THROW(readClusters(n, 0x90ABCDEF, &loc[0]));

    waitForThisBackendWrite(vol_);
    {

        SCOPED_BLOCK_BACKEND(vol_);
        EXPECT_THROW(writeClusters(n, 0x1234567, &loc[0]), TransientException);
    }

    //    unblockBackendWrites();
}

TEST_P(DataStoreNGTest, overflowCache2)
{
    // cf. VOLDRV-128:
    // force SCO rollover before doing actual I/O. this used to trigger an ASSERT,
    // but should just throw a TransientException
    SCOPED_BLOCK_BACKEND(vol_);

    const size_t scosize = vol_->getClusterMultiplier() * vol_->getClusterSize();
    const size_t num_scos = (sc_mp1_size_ + sc_mp2_size_) / scosize;
    std::vector<ClusterLocation> locs((num_scos + 1) * vol_->getClusterMultiplier());
    const size_t num_clusters_first_pass = num_scos * vol_->getClusterMultiplier() - 1;
    const size_t num_clusters_second_pass = locs.size() - num_clusters_first_pass;

    EXPECT_GT(num_clusters_second_pass, (size_t)vol_->getClusterMultiplier());

    EXPECT_NO_THROW(writeClusters(num_clusters_first_pass,
                                  0x12345678,
                                  &locs[0],
                                  0));

    EXPECT_THROW(writeClusters(num_clusters_second_pass,
                               0x9ABCDEF0,
                               &locs[num_clusters_first_pass],
                               0),
                 TransientException);

    readClusters(num_clusters_first_pass,
                 0x12345678,
                 &locs[0],
                 true,
                 0);

    EXPECT_THROW(readClusters(num_clusters_second_pass,
                              0x9ABCDEF0,
                              &locs[num_clusters_first_pass],
                              true,
                              0),
                 std::exception);
}

TEST_P(DataStoreNGTest, destroyCache)
{
    int n = (sc_mp1_size_ + sc_mp2_size_) / dStore_->getClusterSize() - 1;
    std::vector<ClusterLocation> loc(n);

    // Z42: fix by using VolumeTestSetup / VolManagerTestSetup for
    // these tests!!!

    EXPECT_NO_THROW(writeClusters(n, 0x90ABCDEF, &loc[0]));
    waitForThisBackendWrite(vol_);

    EXPECT_NO_THROW(dStore_->destroy(DeleteLocalData::T));

    SCONameList scoList;

    // non disposable SCOs
    dStore_->listSCOs(scoList, false);
    EXPECT_EQ(0U, scoList.size());

    // disposable SCOs
    dStore_->listSCOs(scoList, true);
    EXPECT_EQ(0U, scoList.size());
}

TEST_P(DataStoreNGTest, corruptedReadSCO)
{
    uint64_t clustSize = dStore_->getClusterSize();
    int numClusters = dStore_->getSCOMultiplier().t + 1;

    ASSERT_LT(1, numClusters);

    uint32_t pattern = 0xdeadbeef;

    std::vector<ClusterLocation> locs(numClusters);
    ASSERT_NO_THROW(writeClusters(numClusters,
                                  pattern,
                                  &locs[0]));

    waitForThisBackendWrite(vol_);

    truncate_sco_in_cache(locs[0].sco(),
                          clustSize);

    // partial read will paper over it
    ASSERT_NO_THROW(readClusters(numClusters,
                                 pattern,
                                 &locs[0]));

    // there will be nothing to truncate in the cache if the read
    // was a real partial read (vs. an emulated one).
    truncate_sco_in_cache(locs[0].sco(),
                          clustSize,
                          false);

    truncate_sco_in_backend(locs[0].sco(),
                            clustSize);

    ASSERT_THROW(readClusters(numClusters,
                              pattern,
                              &locs[0]),
                 std::exception);
}

TEST_P(DataStoreNGTest, corruptedReadSCO2)
{
    SCOPED_BLOCK_BACKEND(vol_);

    event_collector_->clear();

    uint64_t clustSize = dStore_->getClusterSize();
    int numClusters = dStore_->getSCOMultiplier().t + 1;

    ASSERT_LT(1, numClusters);

    uint32_t pattern = 0xdeadbeef;

    std::vector<ClusterLocation> locs(numClusters);
    ASSERT_NO_THROW(writeClusters(numClusters,
                                  pattern,
                                  &locs[0]));

    truncate_sco_in_cache(locs[0].sco(),
                          clustSize);

    EXPECT_THROW(readClusters(numClusters,
                              pattern,
                              &locs[0]),
                 std::exception);

    EXPECT_LT(0U,
              event_collector_->size());
}

TEST_P(DataStoreNGTest, corruptedWriteSCO)
{
    event_collector_->clear();

    uint64_t clustSize = dStore_->getClusterSize();
    int numClusters = dStore_->getSCOMultiplier().t - 1;

    ASSERT_LT(1, numClusters);

    uint32_t pattern = 0xdeadbeef;

    std::vector<ClusterLocation> locs(numClusters);
    ASSERT_NO_THROW(writeClusters(numClusters,
                                  pattern,
                                  &locs[0]));

    truncate_sco_in_cache(locs[0].sco(),
                          clustSize);

    EXPECT_THROW(readClusters(numClusters,
                              pattern,
                              &locs[0]),
                 fungi::IOException);

    EXPECT_LT(0U, event_collector_->size());
}

TEST_P(DataStoreNGTest, fakeRead)
{
    ClusterLocation loc(1);

    EXPECT_NO_THROW(dStore_->touchCluster(loc));

    writeClusters(1, 0xbeefcafe, &loc);



    // passing in a 0 ptr as backendinterface is brittle
    CachedSCOPtr sco(dStore_->getSCO( loc.sco(), 0));

    float oldxv = sco->getXVal();
    dStore_->touchCluster(loc);

    EXPECT_TRUE(oldxv < sco->getXVal());
}

TEST_P(DataStoreNGTest, checksum)
{
    const size_t size = vol_->getClusterSize() * (++vol_->getSCOMultiplier());

    const std::string pattern("blah");

    std::vector<byte> buf(vol_->getClusterSize() * vol_->getSCOMultiplier());
    for (size_t i = 0; i < buf.size(); ++i)
    {
        buf[i] = pattern[i % pattern.size()];
    }

    CheckSum expected;
    expected.update(&buf[0], buf.size());

    ClusterLocation loc(1);
    {
        SCOPED_BLOCK_BACKEND(vol_);

        writeToVolume(vol_,
                      0,
                      size,
                      pattern);

        //EXPECT_EQ(expected, dStore_->getCheckSum(loc.sco()));
    }

    syncToBackend(vol_);

    // EXPECT_THROW(dStore_->getCheckSum(loc.sco()),
    //              CheckSumStoreException);
}

TEST_P(DataStoreNGTest, removeSCO)
{
    ClusterLocation loc;

    const uint32_t pattern = 0x12345678;

    writeClusters(1, pattern, &loc);

    EXPECT_THROW(dStore_->removeSCO(loc.sco(), false),
                 std::exception) << "cannot remove current sco which is nondisposable";

    readClusters(1, pattern, &loc);

    dStore_->removeSCO(loc.sco(), true);
    //DataStoreCheckSumStore& checksums = getCheckSumStore();
    // EXPECT_THROW(checksums.find(loc.sco()),
    //              CheckSumStoreException) << "SCO removal must not leak checksums";

    SCOCache* sc = VolManager::get()->getSCOCache();

    EXPECT_TRUE(nullptr == sc->findSCO(getNamespace(),
                                       loc.sco())) <<
        "current sco should be forcibly removable";

    ClusterLocation loc2 = loc;
    writeClusters(1, pattern, &loc);
    EXPECT_EQ(loc2.number() + 1, loc.number());
    EXPECT_EQ(0U, loc.offset());

    //checksums.find(loc.sco());

    syncToBackend(vol_);
    dStore_->writtenToBackendUpTo(loc.sco());
    //EXPECT_THROW(checksums.find(loc.sco()),
    //          CheckSumStoreException) <<
    // "checksums are not kept for disposable SCOs";

    dStore_->removeSCO(loc.sco(), false);

    EXPECT_TRUE(nullptr == sc->findSCO(getNamespace(),
                                       loc.sco())) <<
        "disposable SCO not removed as expected";
    readClusters(1, pattern, &loc);
}

/*
  TEST_P(DataStoreNGTest, removeSCOsForSnapshotRestore)
  {
  size_t numscos = 3;
  size_t numclusters = numscos * vol_->getSCOMultiplier() - 1;
  std::vector<ClusterLocation> locs(numclusters);
  std::vector<SCOName> sconames(numscos);
  const uint32_t pattern = 0xdeadbeef;

  writeClusters(numclusters, pattern, &locs[0]);

  SCOName sconame = 0;
  size_t j = 0;
  for (size_t i = 0; i < numclusters; ++i)
  {
  if (locs[i].getSCOName() != sconame)
  {
  sconames[j++] = sconame = locs[i].getSCOName();
  }
  }

  EXPECT_EQ(numscos, j);

  DataStoreCheckSumStore& checksums = getCheckSumStore();

  BOOST_FOREACH(SCOName& sconame, sconames)
  {
  checksums.find(sconame);
  }

  waitForThisBackendWrite(vol_);
  dStore_->writtenToBackendUpTo(sconames[0]);

  EXPECT_THROW(checksums.find(sconames[0]),
  CheckSumStoreException);

  dStore_->removeSCOsForSnapshotRestore(sconames);

  SCOCache* sc = VolManager::get()->getSCOCache();

  BOOST_FOREACH(SCOName& sconame, sconames)
  {
  EXPECT_THROW(sc->findSCO(getNamespace(),
  sconame),
  std::exception);
  EXPECT_THROW(checksums.find(sconame),
  CheckSumStoreException);
  }

  ClusterLocation loc;
  const uint32_t pattern2 = 0xdeadcafe;

  writeClusters(1, pattern2, &loc);
  EXPECT_EQ(locs.back().number() + 1, loc.number());
  EXPECT_EQ(0, loc.offset());
  }
*/

TEST_P(DataStoreNGTest, writeClusterToLocation)
{
    ClusterLocation loc;
    const uint32_t pattern = 0xabcd;

    writeClusters(1, pattern, &loc);

    std::vector<byte> buf(vol_->getClusterSize());
    {
        ClusterLocation loc2(loc);
        uint32_t throttle;

        EXPECT_THROW(dStore_->writeClusterToLocation(&buf[0],
                                                     loc2,
                                                     throttle),
                     std::exception) <<
            "overwriting an existing location must fail";

        readClusters(1, pattern, &loc2);
    }

    {
        ClusterLocation loc2(loc.number(),
                             loc.offset() + 2,
                             loc.cloneID());

        uint32_t throttle;
        EXPECT_THROW(dStore_->writeClusterToLocation(&buf[0],
                                                     loc2,
                                                     throttle),
                     std::exception) <<
            "writes must be adjacent to the DataStore's current location";
    }

    {
        ClusterLocation loc2(loc.number() + 2,
                             0,
                             loc.cloneID());
        uint32_t throttle;

        EXPECT_THROW(dStore_->writeClusterToLocation(&buf[0],
                                                     loc2,
                                                     throttle),
                     std::exception) <<
            "writes must be adjacent to the DataStore's current location";
    }

    {
        ClusterLocation loc2(loc.number(),
                             loc.offset() + 1,
                             loc.cloneID());
        uint32_t throttle;

        dStore_->writeClusterToLocation(&buf[0],
                                        loc2,
                                        throttle);

        std::vector<byte> rbuf(buf.size());
        readCluster(&rbuf[0], loc2, 0);
        EXPECT_EQ(0, memcmp(&rbuf[0], &buf[0], buf.size()));
    }

    {
        ClusterLocation loc2(loc.number() + 1,
                             0,
                             loc.cloneID());

        uint32_t throttle;

        EXPECT_THROW(dStore_->writeClusterToLocation(&buf[0],
                                                     loc2,
                                                     throttle),
                     std::exception);

        dStore_->finalizeCurrentSCO();
        dStore_->writeClusterToLocation(&buf[0],
                                        loc2,
                                        throttle);

        std::vector<byte> rbuf(buf.size());
        readCluster(&rbuf[0], loc2, 0);
        EXPECT_EQ(0, memcmp(&rbuf[0], &buf[0], buf.size()));
    }
}

TEST_P(DataStoreNGTest, writtenToBackendUpTo)
{
    size_t numscos = 65;
    size_t numclusters = (numscos - 1) * vol_->getSCOMultiplier() + 1;
    std::vector<ClusterLocation> locs(numclusters);

    SCOPED_BLOCK_BACKEND(vol_);

    writeClusters(numclusters,
                  0xdeadbeef,
                  &locs[0]);

    SCO first = locs[0].sco();
    SCO last = first;
    dStore_->writtenToBackend(last);

    for (size_t i = 0; i < numclusters - 1; ++i)
    {
        SCO sconame = locs[i].sco();
        if (sconame != last)
        {
            dStore_->writtenToBackend(sconame);
            last = sconame;
        }
    }

    EXPECT_TRUE(last != first);
    SCO sconame(numscos / 2);
    dStore_->writtenToBackendUpTo(sconame);

    SCONameList scos;
    dStore_->listSCOs(scos, true);

    EXPECT_EQ(sconame.number(), scos.size());
    BOOST_FOREACH(SCO n, scos)
    {
        EXPECT_GE(sconame.number(), n.number());
    }

    scos.clear();

    dStore_->listSCOs(scos, false);
    EXPECT_EQ(numscos - sconame.number(), scos.size());
    BOOST_FOREACH(SCO n, scos)
    {
        EXPECT_LT(sconame.number(), n.number());
    }

    // clear the write sco tasks from the queue, the bsemtask will lead to it
    // throwing
    EXPECT_THROW(VolManager::get()->backend_thread_pool()->stop(vol_, 1),
                 std::exception);
}

TEST_P(DataStoreNGTest, scoFullTest)
{
    uint32_t throttle;

    std::vector<byte> buf(dStore_->getClusterSize());
    std::vector<ClusterLocation> loc(1);
    size_t cluster_cnt = dStore_->getSCOMultiplier().t - 1;
    for (uint64_t i = 0; i < cluster_cnt; ++i)
    {
        dStore_->writeClusters(&buf[0], loc, loc.size(), throttle);
        EXPECT_LT(0,
                  dStore_->getRemainingSCOCapacity());
    }

    dStore_->writeClusters(&buf[0], loc, loc.size(), throttle);
    EXPECT_EQ(0U,
              dStore_->getRemainingSCOCapacity());

    MaybeCheckSum cs = dStore_->finalizeCurrentSCO();
    EXPECT_TRUE(static_cast<bool>(cs));

    loc[0].incrementNumber();

    ClusterLocation& loco = loc[0];

    for (uint64_t i = 0; i < cluster_cnt; ++i)
    {
        loco.offset(i);
        dStore_->writeClusterToLocation(&buf[0], loco, throttle);
        EXPECT_LT(0,
                  dStore_->getRemainingSCOCapacity());
    }

    loco.incrementOffset();
    dStore_->writeClusterToLocation(&buf[0], loco, throttle);
    EXPECT_EQ(0U,
              dStore_->getRemainingSCOCapacity());
}

// VOLDRV-143
TEST_P(DataStoreNGTest, errorOnFinalizeSCO)
{
    std::vector<ClusterLocation> locs(3 * vol_->getClusterMultiplier());
    const uint32_t pattern = 0xbeefcafe;

    // don't fill it up completely as writeClusters does SCO rollover in that case
    EXPECT_NO_THROW(writeClusters(vol_->getClusterMultiplier() - 1,
                                  pattern,
                                  &locs[0]));

    // break all mountpoints except the current one - finalizeCurrentSCO() which tries
    // to create a new one on another mountpoint, must not fail and must return a
    // valid checksum
    {
        CachedSCOPtr sco_ptr = getCurrentSCO(vol_);
        SCOCache::SCOCacheMountPointList& l = getSCOCacheMountPointList();
        BOOST_FOREACH(SCOCacheMountPointPtr mp, l)
        {
            if (mp != sco_ptr->getMountPoint())
            {
                breakMountPoint(mp, vol_->getNamespace());
            }
        }
    }

    MaybeCheckSum cs;
    EXPECT_NO_THROW(cs = dStore_->finalizeCurrentSCO());
    EXPECT_TRUE(static_cast<bool>(cs));

    // we still have a mountpoint and locs left, so let's put 'em to work
    writeClusters(locs.size() - vol_->getClusterMultiplier() + 1,
                  pattern,
                  &locs[vol_->getClusterMultiplier() - 1]);

    readClusters(locs.size(),
                 pattern,
                 &locs[0]);
}

INSTANTIATE_TEST(DataStoreNGTest);

}

// Local Variables: **
// mode: c++ **
// End: **
