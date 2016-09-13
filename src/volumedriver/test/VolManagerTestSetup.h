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

#ifndef VOLMANAGER_TEST_SETUP_H
#define VOLMANAGER_TEST_SETUP_H

#include "EventCollector.h"
#include "VolumeDriverTestConfig.h"
#include "FailOverCacheTestSetup.h"
#include "MetaDataStoreTestSetup.h"

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

#include <youtils/Logging.h>
#include <youtils/ArakoonTestSetup.h>

#include <backend/BackendConfig.h>
#include <backend/BackendTestSetup.h>

#include <volumedriver/MetaDataBackendConfig.h>
#include <volumedriver/TransientException.h>
#include <volumedriver/Types.h>
#include <volumedriver/VolManager.h>
#include <volumedriver/Volume.h>
#include <volumedriver/VolumeConfig.h>

#include "fawlty.hh"
#include <pstreams/pstream.h>
#include <fawltyfs/FileSystem.h>

namespace metadata_server
{

class ServerConfig;

}

namespace volumedriver
{

class SCOCache;
class SnapshotManagement;

VD_BOOLEAN_ENUM(UseFawltyMDStores);
VD_BOOLEAN_ENUM(UseFawltyTLogStores);
VD_BOOLEAN_ENUM(UseFawltyDataStores);
VD_BOOLEAN_ENUM(CheckVolumeNameForRestart);

namespace fs = boost::filesystem;
namespace be = backend;

class BackendInterfaceConfig;
class BSemTask;
class ScopedBackendBlocker;
class UnblockInFuture;

class VolumeCheckerThread
{
public:
    VolumeCheckerThread(Volume* v,
                        const std::string& pattern,
                        size_t block_size = 4096,
                        size_t read_size = 4096,
                        unsigned retries = 10);

    VolumeCheckerThread(const VolumeCheckerThread&) = delete;
    VolumeCheckerThread& operator=(const VolumeCheckerThread) = delete;

    ~VolumeCheckerThread();

    void

    waitForFinish();

private:
    DECLARE_LOGGER("VolumeCheckerThread");

    Volume* v_;
    const std::string pattern_;
    std::vector<byte> buf_;
    size_t read_size_;
    unsigned retries_;
    std::unique_ptr<boost::thread> thread_;

    void
    run();
};

// Z42:
// would be nice to have configurable:
// - read/write ratio
// - io pattern (sequential vs. randomness) for reads and writes
// - read/write ordering (first write then read, random)
// - io sizes (fixed vs. random)
// - duration
VD_BOOLEAN_ENUM(CheckReadResult)

class VolumeRandomIOThread
{
public:
    VolumeRandomIOThread(Volume& v,
                         const std::string &writePattern,
                         const CheckReadResult = CheckReadResult::F);

    VolumeRandomIOThread(const VolumeRandomIOThread&) = delete;
    VolumeRandomIOThread& operator=(const VolumeRandomIOThread&) = delete;

    ~VolumeRandomIOThread();

    void stop();

 private:
    Volume& vol_;
    std::unique_ptr<boost::thread> thread_;
    const std::string writePattern_;
    bool stop_;
    size_t bufsize_;
    const CheckReadResult check_;
    std::vector<uint8_t> bufv_;

    void run();
};

class VolumeWriteThread
{
public:
    VolumeWriteThread(Volume& v,
                      const std::string& pattern,
                      double throughput,
                      uint64_t max_writes = 0);

    VolumeWriteThread(const VolumeWriteThread&) = delete;
    VolumeWriteThread& operator=(const VolumeWriteThread&) = delete;

    void
    stop();

    ~VolumeWriteThread();

    void
    waitForFinish();


private:
    DECLARE_LOGGER("VolumeWriteThread");

    std::unique_ptr<boost::thread> thread_;
    bool stop_;
    const std::string pattern_;
    Volume& vol_;
    uint64_t max_writes_;
    uint64_t written_;

    const static uint32_t bufsize ;

    bool maxNotExceeded() const
    {
        return (max_writes_ == 0
                or  written_ < max_writes_);
    }

    void
    run();
};

struct VolManagerTestSetupParameters
{
#define PARAM(type, name)                       \
    const type&                                 \
    name() const                                \
    {                                           \
        return name ## _;                       \
    }                                           \
                                                \
    VolManagerTestSetupParameters&              \
    name(const type& val)                       \
    {                                           \
        name ## _ = val;                        \
        return *this;                           \
    }                                           \
                                                \
    type name ## _

    PARAM(UseFawltyMDStores, use_fawlty_md_stores) = UseFawltyMDStores::F;
    PARAM(UseFawltyTLogStores, use_fawlty_tlog_stores) = UseFawltyTLogStores::F;
    PARAM(UseFawltyDataStores, use_fawlty_data_stores) = UseFawltyDataStores::F;
    PARAM(size_t, backend_threads) = 4;
    PARAM(std::string, sco_cache_mp1_size) = "1GiB";
    PARAM(std::string, sco_cache_mp2_size) = "1GiB";
    PARAM(std::string, sco_cache_trigger_gap) = "250MiB";
    PARAM(std::string, sco_cache_backoff_gap) = "500MiB";
    PARAM(uint32_t, sco_cache_cleanup_interval) = 60;
    PARAM(size_t, open_scos_per_volume) = 32;
    PARAM(size_t, data_store_throttle_usecs) = 4000;
    PARAM(size_t, dtl_throttle_usecs) = 10000;
    PARAM(uint32_t, scos_per_tlog) = 20;
    PARAM(boost::chrono::seconds, dtl_check_interval) = boost::chrono::seconds(3600);
#undef PARAM

    const std::string name_;

    const std::string&
    name() const
    {
        return name_;
    }

    // purposefully not 'explicit' so tests that use the defaults
    // still compile with VolManagerTestSetup("SomeName")
    VolManagerTestSetupParameters(const char* c)
        : name_(c)
    {}

    VolManagerTestSetupParameters(const std::string& n)
        : name_(n)
    {}

    ~VolManagerTestSetupParameters() = default;

    VolManagerTestSetupParameters(const VolManagerTestSetupParameters&) = default;

    VolManagerTestSetupParameters&
    operator=(const VolManagerTestSetupParameters&) = default;
};

class VolManagerTestSetup
    : public testing::TestWithParam<VolumeDriverTestConfig>
    , public be::BackendTestSetup
    , public volumedrivertest::FailOverCacheTestSetup
{
    friend class ScopedBackendBlocker;
    template<typename> friend class FutureUnblocker;
    friend class FutureUnblockerWriteOnlyVolume;

    friend class UnblockInFuture;
    friend int main(int, char**);

public:
    // purposefully not 'explicit' so tests that use the defaults
    // still compile with VolManagerTestSetup("SomeName")
    VolManagerTestSetup(const VolManagerTestSetupParameters&);

    virtual ~VolManagerTestSetup();

    virtual void
    SetUp();

    virtual void
    TearDown();

    template<typename T>
    static void
    writeClusters(T& vol,
                  uint64_t number_of_clusters,
                  const std::string& basepattern = "X",
                  uint64_t pattern_period = 0)
    {
        for (uint64_t i = 0; i < number_of_clusters; i++)
        {
            uint64_t lba = (i * vol.getClusterMultiplier()) % vol.getLBACount();
            std::stringstream ss;
            ss << basepattern << ( pattern_period ? i % pattern_period : i);
            writeToVolume(vol, lba, vol.getClusterSize(), ss.str());
        }
    }

    template<typename T>
    static void
    checkClusters(T& vol,
                  uint64_t number_of_clusters,
                  const std::string& basepattern = "X",
                  uint64_t pattern_period = 0)
    {
        checkClusters(vol,
                      0,
                      number_of_clusters,
                      basepattern,
                      pattern_period);
    }

    template<typename T>
    static void
    checkClusters(T& vol,
                  uint64_t offset,
                  uint64_t number_of_clusters,
                  const std::string& basepattern = "X",
                  uint64_t pattern_period = 0)
    {
        for (uint64_t i = offset; i < number_of_clusters + offset; i++)
        {
            uint64_t lba = (i * vol.getClusterMultiplier()) % vol.getLBACount();
            std::stringstream ss;
            ss << basepattern << ( pattern_period ? i % pattern_period : i);
            checkVolume(vol, lba, vol.getClusterSize(), ss.str());
        }
    }

    template<typename T>
    static void
    writeToVolume(T& vol,
                  const std::string& pattern,
                  uint64_t blocksize = 4096,
                  unsigned retries = 5)
    {
        for (size_t i = 0; i < vol.getSize(); i += blocksize)
        {
            writeToVolume(vol, i / vol.getLBASize(), blocksize, pattern, retries);
        }
    }

    template<typename T>
    static void
    writeToVolume(T& volume,
                  uint64_t lba,
                  uint64_t size,
                  const std::string& pattern,
                  unsigned retries=5)
    {
        if(pattern.empty())
        {
            LOG_ERROR("Cannot write an empty pattern");
            throw fungi::IOException("pass pattern correctly please");
        }

        std::vector<uint8_t> buf(size);

        for (uint64_t i = 0; i < size; ++i)
        {
            buf[i] = pattern[i%pattern.size()];
        }

        writeToVolume(volume, lba, size, &buf[0], retries);
    }

    template<typename T>
    static void
    writeToVolume(T& volume,
                  uint64_t lba,
                  uint64_t size,
                  const uint8_t *buf,
                  unsigned retries = 5)
    {
        while (true)
        {
            try
            {
                volume.write(lba, buf, size);
                break;
            }
            catch (TransientException &)
            {
                LOG_INFO("Backend congestion detected - retrying");
                if (retries--)
                {
                    sleep(1);
                }
                else
                {
                    LOG_FATAL("Backend congestion persists - giving up");
                    throw;
                }
            }
        }
    }

    VolumeSize
    default_volume_size() const
    {
        return VolumeSize((1 << 18) * 512);
    }

    SCOMultiplier
    default_sco_multiplier() const
    {
        return SCOMultiplier(32);
    }

    LBASize
    default_lba_size() const
    {
        return LBASize(VolumeConfig::default_lba_size_);
    }

    ClusterMultiplier
    default_cluster_multiplier() const
    {
        return GetParam().cluster_multiplier();
    }

    ClusterSize
    default_cluster_size() const
    {
        return ClusterSize(default_lba_size() * default_cluster_multiplier());
    }

    SharedVolumePtr
    newVolume(const VanillaVolumeConfigParameters& params);

public:
    SharedVolumePtr
    newVolume(const backend::BackendTestSetup::WithRandomNamespace& wrns,
              const boost::optional<VolumeSize>& volume_size = boost::none,
              const boost::optional<SCOMultiplier>& sco_multiplier = boost::none,
              const boost::optional<LBASize>& lba_size = boost::none,
              const boost::optional<ClusterMultiplier>& cluster_mult = boost::none,
              const boost::optional<size_t>& num_cached_pages = boost::none);

    SharedVolumePtr
    newVolume(const std::string& volumeName,
              const backend::Namespace& namespc,
              const boost::optional<VolumeSize>& volume_size = boost::none,
              const boost::optional<SCOMultiplier>& sco_multiplier = boost::none,
              const boost::optional<LBASize>& lba_size = boost::none,
              const boost::optional<ClusterMultiplier>& cluster_mult = boost::none,
              const boost::optional<size_t>& num_cached_pages = boost::none);

    WriteOnlyVolume*
    newWriteOnlyVolume(const std::string& volumeName,
                       const backend::Namespace& namespc,
                       const VolumeConfig::WanBackupVolumeRole role,
                       const boost::optional<VolumeSize>& volume_size = boost::none,
                       const boost::optional<SCOMultiplier>& sco_multiplier = boost::none,
                       const boost::optional<LBASize>& lba_size = boost::none,
                       const boost::optional<ClusterMultiplier>& cluster_mult = boost::none);

    void
    set_cluster_cache_default_behaviour(const ClusterCacheBehaviour);

    void
    set_cluster_cache_default_mode(const ClusterCacheMode);

    SharedVolumePtr
    localRestart(const backend::Namespace& namespc,
                 const FallBackToBackendRestart = FallBackToBackendRestart::F);

    void
    setVolumeRole(const Namespace ns,
                  VolumeConfig::WanBackupVolumeRole);

    void
    restartVolume(const VolumeConfig& cfg,
                  const PrefetchVolumeData = PrefetchVolumeData::F,
                  const CheckVolumeNameForRestart = CheckVolumeNameForRestart::T,
                  const IgnoreFOCIfUnreachable = IgnoreFOCIfUnreachable::F);

    WriteOnlyVolume*
    restartWriteOnlyVolume(const VolumeConfig&);

    fs::path
    setupClusterCacheDevice(const std::string& name,
                         int size);

    SharedVolumePtr
    getVolume(const VolumeId & name);

    SharedVolumePtr
    createClone(const backend::BackendTestSetup::WithRandomNamespace& wrns,
                const backend::Namespace& parentVolNamespace,
                const boost::optional<SnapshotName>& snapshot);

    SharedVolumePtr
    createClone(const std::string& cloneName,
                const backend::Namespace& newNamespace,
                const backend::Namespace& parentVolNamespace,
                const boost::optional<SnapshotName>& snapshot);

    void
    fill_backend_cache(const backend::Namespace& ns);

    // THIS COULD BE MADE A TEMPLATE AGAIN WITH VARIABLE ARGS TEMPLATES
    void
    destroyVolume(WriteOnlyVolume* v,
                  const RemoveVolumeCompletely /*cleanup_backend*/);

    void
    destroyVolume(SharedVolumePtr& v,
                  const DeleteLocalData delete_local_data,
                  const RemoveVolumeCompletely remove_volume_completely);

    void
    putVolume(Volume * v);

    void
    writeSnapshotFileToBackend(Volume& v);

    static void
    checkVolume(Volume& volume,
                uint64_t lba,
                uint64_t block_size,
                const std::string& pattern = "",
                unsigned retries = 10);

    static void
    checkVolume(Volume& v,
                const std::string& pattern,
                uint64_t block_size = 4096,
                unsigned retries = 10);

    static void
    checkVolume_aux(Volume& v,
                    const std::string& pattern,
                    uint64_t size,
                    uint64_t block_size = 4096,
                    unsigned retries = 10);

    void
    readVolume(Volume& vol,
               uint64_t block_size,
               unsigned retries = 10);

    void
    checkCurrentBackendSize(Volume& vol);

    void
    waitForThisBackendWrite(VolumeInterface&);

    bool
    isVolumeSyncedToBackend(Volume& v);

    void
    scheduleBackendSync(Volume& v);

    void
    syncToBackend(Volume& v);

    void
    syncMetaDataStoreBackend(Volume& v);

    void
    scheduleBarrierTask(VolumeInterface&);

    void
    scheduleThrowingTask(Volume&);

    void
    removeNonDisposableSCOS(Volume& v);

    void
    createSnapshot(Volume&, const std::string &name);

    void
    restoreSnapshot(Volume&, const std::string &name);

    fs::path
    getVolDir(const std::string &volName) const;

    fs::path
    getCurrentTLog(const VolumeId& volid) const;

    void
    getTLogsNotInBackend(const VolumeId& volid,
                     OrderedTLogIds& tlogs) const;

    fs::path
    getDCDir(const std::string& volName) const;

    SnapshotManagement*
    getSnapshotManagement(Volume& vol);

    void
    temporarilyStopVolManager();

    void
    startVolManager();

    void
    restartVolManager(const boost::property_tree::ptree&);

    static void
    setFailOverCacheConfig(const VolumeId&,
                           const boost::optional<FailOverCacheConfig>&);

    void
    flushFailOverCache(Volume&);

    FailOverCacheClientInterface*
    getFailOverWriter(Volume& vol);

    bool
    checkVolumesIdentical(Volume& v1,
                          Volume& v2) const;

    const CachedSCOPtr
    getCurrentSCO(Volume&);

    void
    persistXVals(const VolumeId& volname) const;

    void
    updateReadActivity() const;

    DECLARE_LOGGER("VolManagerTestSetup");

    static VolumeDriverTestConfig
    default_test_config();

    void
    waitForPrefetching(Volume&) const;

    SharedVolumePtr
    findVolume(const VolumeId& ns);

    void
    setNamespaceRestarting(const Namespace& ns,
                           const VolumeConfig& volume_config);

    void
    clearNamespaceRestarting(const Namespace& ns);

    void
    breakMountPoint(SCOCacheMountPointPtr mp,
                    const backend::Namespace& nspace);

    SCOCache::SCOCacheMountPointList&
    getSCOCacheMountPointList();

    SCOCache::NSMap&
    getSCOCacheNamespaceMap();

    void
    apply_scrub_reply(Volume&,
                      const scrubbing::ScrubReply&,
                      const ScrubbingCleanup = ScrubbingCleanup::Always);

    static youtils::Weed
    growWeed();

    CORBA::ORB_var orb_;
    Fawlty::FileSystemFactory_var fawlty_ref_;

    Fawlty::FileSystem_var tlogs_;
    Fawlty::FileSystem_var mdstores_;
    Fawlty::FileSystem_var sdstore1_;
    Fawlty::FileSystem_var sdstore2_;

protected:
    // Strong Type These
    void
    add_failure_rule(Fawlty::FileSystem_var& fawlty_fs,
                     unsigned rule_id,
                     const std::string& path_regex,
                     const std::set<fawltyfs::FileSystemCall>& calls,
                     int ramp_up,
                     int duration,
                     int error_code);

    void
    remove_failure_rule(Fawlty::FileSystem_var& fawlty_fs,
                        unsigned rule_id);

    const std::string testName_;
    const boost::filesystem::path directory_;
    const fs::path configuration_;

    uint64_t lba_cnt_;

    // cluster cache
    fs::path cc_mp1_path_;
    fs::path cc_mp2_path_;

    // sco cache
    fs::path sc_mp1_path_;
    fs::path sc_mp2_path_;

    const uint64_t sc_mp1_size_;
    const uint64_t sc_mp2_size_;
    const uint64_t sc_trigger_gap_;
    const uint64_t sc_backoff_gap_;
    const uint32_t sc_clean_interval_;

    unsigned open_scos_per_volume_;
    unsigned dstore_throttle_usecs_;
    unsigned foc_throttle_usecs_;

    void
    getCacheConfig(boost::property_tree::ptree&) const;

    unsigned
    getGlobalFDEstimate() const;

    unsigned
    getVolumeFDEstimate() const;

    void
    writeVolumeConfigToBackend(Volume&) const;

    volumedriver::MetaDataBackendType
    metadata_backend_type() const;

    uint64_t
    volume_potential_sco_cache(const ClusterSize,
                               const SCOMultiplier,
                               const boost::optional<TLogMultiplier>&);

    static boost::shared_ptr<backend::Condition>
    claim_namespace(const backend::Namespace&,
                    const OwnerTag);

    static OwnerTag
    new_owner_tag();

    const UseFawltyMDStores useFawltyMDStores_;
    const UseFawltyTLogStores useFawltyTLogStores_;
    const UseFawltyDataStores useFawltyDataStores_;

    const boost::chrono::seconds dtl_check_interval_;

    std::shared_ptr<arakoon::ArakoonTestSetup> arakoon_test_setup_;
    std::shared_ptr<volumedrivertest::MDSTestSetup> mds_test_setup_;
    std::unique_ptr<metadata_server::ServerConfig> mds_server_config_;
    std::unique_ptr<volumedrivertest::MetaDataStoreTestSetup> mdstore_test_setup_;
    std::shared_ptr<volumedrivertest::EventCollector> event_collector_;

private:
    CORBA::Object_ptr
    getObjectReference(CORBA::ORB_ptr orb);

    bool volManagerRunning_;

    std::map<VolumeInterface*, BSemTask*> blockers_;

    const unsigned num_threads_;
    const uint32_t num_scos_in_tlog_;

    redi::ipstream pstream_;

    static void
    readVolume_(Volume& vol,
                uint64_t lba,
                uint64_t block_size,
                const std::string* const pattern,
                unsigned retries);

    void
    blockBackendWrites_(VolumeInterface* v);

    void
    unblockBackendWrites_(VolumeInterface* v);

    fungi::Thread*
    futureUnblockBackendWrites(VolumeInterface* v, unsigned secs);

    void
    setup_fawlty_directory(fs::path dir,
                           Fawlty::FileSystem_var& var,
                           const std::string& name);

    void
    setup_stores_();

    void
    teardown_stores_();

    void
    setup_corba_();

    void
    setup_md_backend_();

    void
    teardown_md_backend_();

    void
    setup_arakoon_();

    void
    teardown_arakoon_();
};

class ScopedBackendBlocker
{
public:
    ScopedBackendBlocker(VolManagerTestSetup* t,
                         VolumeInterface& v)
        : t_(t)
        , v_(v)
    {
        t_->blockBackendWrites_(&v_);
    }

    ~ScopedBackendBlocker()
    {
        t_->unblockBackendWrites_(&v_);
    }

private:
    VolManagerTestSetup* t_;
    VolumeInterface& v_;
};

#define SCOPED_BLOCK_BACKEND(v) ScopedBackendBlocker __b(this, v)

template<typename V>
class FutureUnblocker
{
public:
    FutureUnblocker(VolManagerTestSetup* t,
                    SharedVolumePtr v,
                    unsigned tm,
                    const DeleteLocalData cleanupLocal,
                    const RemoveVolumeCompletely cleanupBackend)
        : t_(t),
          v_(v),
          tm_(tm),
          cleanupLocal_(cleanupLocal),
          cleanupBackend_(cleanupBackend)

    {
        t_->blockBackendWrites_(v_.get());
    }

    ~FutureUnblocker()
    {
        fungi::Thread* tr = t_->futureUnblockBackendWrites(v_.get(),
                                                           tm_);
        t_->destroyVolume(v_,
                          cleanupLocal_,
                          cleanupBackend_);
        tr->join();
        tr->destroy();
    }

private:
    VolManagerTestSetup* t_;
    SharedVolumePtr v_;
    unsigned tm_;
    const DeleteLocalData cleanupLocal_;
    const RemoveVolumeCompletely cleanupBackend_;
};


class FutureUnblockerWriteOnlyVolume
{
public:
    FutureUnblockerWriteOnlyVolume(VolManagerTestSetup* t,
                                   WriteOnlyVolume* v,
                                   unsigned tm,
                                   const RemoveVolumeCompletely cleanupBackend)
        : t_(t),
          v_(v),
          tm_(tm),
          cleanupBackend_(cleanupBackend)

    {
        t_->blockBackendWrites_(v_);
    }

    ~FutureUnblockerWriteOnlyVolume()
    {
        fungi::Thread* tr = t_->futureUnblockBackendWrites(v_,tm_);
        t_->destroyVolume(v_,
                          cleanupBackend_);
        tr->join();
        tr->destroy();
    }

private:
    VolManagerTestSetup* t_;
    WriteOnlyVolume* v_;
    unsigned tm_;
    const RemoveVolumeCompletely cleanupBackend_;
};

#define SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v, tm, cleanlocal, cleanbackend) \
    FutureUnblocker<Volume> __f(this, v, tm, cleanlocal, cleanbackend)

#define SCOPED_DESTROY_WRITE_ONLY_VOLUME_UNBLOCK_BACKEND_FOR_BACKEND_RESTART(v, tm) \
    FutureUnblockerWriteOnlyVolume __f(this, v, tm, RemoveVolumeCompletely::F)

#define INSTANTIATE_TEST(name)                                          \
    INSTANTIATE_TEST_CASE_P(name##s,                                    \
                            name,                                       \
                            ::testing::Values(::volumedriver::VolManagerTestSetup::default_test_config()))
}

#endif // VOLMANAGER_TEST_SETUP_H

// Local Variables: **
// mode: c++ **
// End: **
