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

#include "DataStoreNG.h"
#include "TracePoints_tp.h"
#include "TransientException.h"
#include "Volume.h"
#include "VolManager.h"

#include <cerrno>
#include <algorithm>

#include <youtils/Assert.h>
#include <youtils/ScopeExit.h>
#include <youtils/Timer.h>

namespace volumedriver
{

using ::youtils::FileUtils;

namespace bc = boost::chrono;
namespace be = backend;
namespace yt = youtils;

#define WLOCK_DATASTORE()                       \
    boost::unique_lock<decltype(rw_lock_)> ulg__(rw_lock_)

#define RLOCK_DATASTORE()                       \
    boost::shared_lock<decltype(rw_lock_)> slg__(rw_lock_)

#define ASSERT_WLOCKED()                        \
    ASSERT(not rw_lock_.try_lock_shared())

#define ASSERT_RLOCKED()                        \
    ASSERT(not rw_lock_.try_lock())

#define SERIALIZE_ERROR_HANDLING()                      \
    boost::lock_guard<decltype(error_lock_)> elg__(error_lock_)

DataStoreNG::DataStoreNG(const VolumeConfig& cfg,
                         SCOCache* scoCache,
                         unsigned num_open_scos)
    : VolumeBackPointer(getLogger__())
    , cluster_size_(cfg.getClusterSize())
    , sco_mult_(cfg.sco_mult_)
    , scoCache_(scoCache)
    , nspace_(cfg.getNS())
    , openSCOs_(num_open_scos)
    , latestSCOnumberInBackend_(0)
    , cacheHitCounter_(0)
    , cacheMissCounter_(0)
    , currentCheckSum_(nullptr)
{
    WLOCK_DATASTORE();
    validateConfig_();
}

void
DataStoreNG::initialize(VolumeInterface* vol)
{
    WLOCK_DATASTORE();
    setVolume(vol);
}

const ClusterLocation&
DataStoreNG::localRestart(uint64_t nspace_min,
                          uint64_t nspace_max,
                          const SCOAccessData& sad,
                          SCONumber lastSCOnumberInBackend,
                          ClusterLocation lastClusterLocation)
{
    WLOCK_DATASTORE();

    VERIFY(currentSCO_() == 0);

    latestSCOnumberInBackend_ = lastSCOnumberInBackend;
    currentClusterLoc_ = ClusterLocation(SCONumber(lastClusterLocation.number() + 1));


    LOG_INFO("Enabling namespace " << nspace_);
    scoCache_->enableNamespace(nspace_,
                               nspace_min,
                               nspace_max,
                               sad);
    {
        SCONameList names;
        scoCache_->getSCONameListAll(nspace_, names);

        for (const auto& sco : names)
        {
            if (sco.cloneID() == 0)
            {
                if (sco.number() > lastClusterLocation.sco().number())
                {
                    LOG_INFO("Removing too recent SCO " << sco
                             << " from namespace " << nspace_);

                    try
                    {
                        removeSCO_(sco, true);
                    }
                    catch (TransientException& e)
                    {
                        std::stringstream ss;
                        ss << "SCO " << sco <<
                            " is being retrieved to namespace " << nspace_ <<
                            " while it is being restarted locally";
                        VERIFY(ss.str() == "");
                    }
                    CATCH_STD_ALL_LOG_IGNORE("Failed to remove SCO " <<
                                             sco << " from namespace " << nspace_);
                }
                else if(sco.number() == lastClusterLocation.sco().number())
                {
                    CachedSCOPtr sco_ptr(scoCache_->findSCO_throw(nspace_,
                                                                  sco));
                    FileUtils::truncate(sco_ptr->path(),
                                        getVolume()->getClusterSize() * (lastClusterLocation.offset() + 1));

                }
            }
        }

        SCONameList nondisposableNames;
        scoCache_->getSCONameList(nspace_,
                                  nondisposableNames,
                                  false);
        for (const auto& sco : nondisposableNames)
        {

            if(sco.cloneID() == 0 and
               sco.number() <= lastSCOnumberInBackend)
            {
                CachedSCOPtr scoptr;
                try
                {
                    scoptr = scoCache_->findSCO_throw(nspace_,
                                                      sco);
                    LOG_INFO("Setting sco "  << sco << " disposable.");
                    scoCache_->setSCODisposable(scoptr);
                }
                CATCH_STD_ALL_LOG_IGNORE("Exception setting SCO " << sco <<
                                         " disposable");
            }
        }
    }

    LOG_INFO(getVolume()->getName() << ": using " << currentClusterLoc_ <<
             " as next cluster location");

    return currentClusterLoc_;
}

void
DataStoreNG::restoreSnapshot(SCONumber num)
{
    WLOCK_DATASTORE();

    latestSCOnumberInBackend_ = num;
    currentClusterLoc_ = ClusterLocation(SCONumber(num+1));
    pendingTLogSCOs_.clear();

    SCONameList names;
    scoCache_->getSCONameListAll(nspace_, names);

    for (const auto& sconame : names)
    {
        if (sconame.cloneID() == 0 and
            sconame.number() > num)
        {
            LOG_INFO("Removing too recent SCO " <<
                     sconame <<
                     " from namespace " << nspace_);

            try
            {
                removeSCO_(sconame, true, false);
            }
            catch (TransientException& e)
            {
                std::stringstream ss;
                ss << "SCO " << sconame <<
                    " is being retrieved to namespace " << nspace_ <<
                    " while the volume is being restored to a snapshot";
                VERIFY(ss.str() == "");
            }
            CATCH_STD_ALL_LOG_IGNORE("Failed to remove SCO " <<
                                     sconame <<
                                     " from namespace " << nspace_);
        }
    }
}

void
DataStoreNG::newVolume(uint64_t nspace_min,
                       uint64_t nspace_max)
{
    WLOCK_DATASTORE();
    VERIFY(currentSCO_() == 0);

    VERIFY(!scoCache_->hasNamespace(nspace_));

    scoCache_->addNamespace(nspace_,
                            nspace_min,
                            nspace_max);

    currentClusterLoc_ = ClusterLocation(SCONumber(1));

    LOG_DEBUG("Creating new write SCO " << currentClusterLoc_);
    try
    {
        updateCurrentSCO_();
    }
    catch (TransientException &)
    {
        // Don't propagate the cache full exception
    }
}

void
DataStoreNG::scanSCOsForBackendRestart_(SCONumber lastSCOInBackend)
{
    LOG_INFO("Scanning SCOs for backend restart");
    SCONameList l;
    scoCache_->getSCONameListAll(nspace_, l);

    for(SCONameList::iterator i = l.begin(); i != l.end(); ++i)
    {
        CachedSCOPtr sco(scoCache_->findSCO(nspace_,
                                            *i));
        if (sco == nullptr)
        {
            continue;
        }

        const bool is_disposable = scoCache_->isSCODisposable(sco);
        const bool is_larger = i->number() > lastSCOInBackend;
        const bool is_from_parent(i->cloneID() != SCOCloneID(0));

        bool delete_sco = false;

        if(is_from_parent)
        {
            if(not is_disposable)
            {

                LOG_WARN(nspace_ << ": SCO " << *i <<
                         " nondisposable and cloneid not zero " <<
                         std::hex << lastSCOInBackend << std::dec <<
                         " - removing it");
                delete_sco = true;
            }
        }
        else
        {
            if(is_larger)
            {
                LOG_WARN(nspace_ << ": SCO " << *i <<
                         " more recent SCONumber than " <<
                         std::hex << lastSCOInBackend << std::dec <<
                         " - removing it");
                delete_sco = true;
            }
            else
            {
                if(not is_disposable)
                {
                    LOG_WARN(nspace_ << ": SCO " << *i <<
                             "nondisposable, removing it");
                    // scoCache_->setSCODisposable(sco);
                    delete_sco = true;
                }
            }
        }
        if(delete_sco)
        {
            while (true)
            {
                try
                {
                    scoCache_->removeSCO(nspace_,
                                         *i,
                                         true);
                    break;
                }
                catch (TransientException& e)
                {
                    LOG_DEBUG(e.what() << ": retrying sco removal");
                    usleep(100000);
                }
            }
        }
    }
}

void
DataStoreNG::backend_restart(const SCONumber lastSCOInBackend,
                             uint64_t nspace_min,
                             uint64_t nspace_max)
{
    WLOCK_DATASTORE();

    VERIFY(currentSCO_() == 0);
    VERIFY(!scoCache_->hasNamespace(nspace_));

    if (scoCache_->hasDisabledNamespace(nspace_))
    {
        LOG_INFO("Renabling namespace" << nspace_);
        BackendInterfacePtr bi(getVolume()->getBackendInterface()->cloneWithNewNamespace(nspace_));
        SCOAccessDataPersistor sadp(std::move(bi));
        SCOAccessDataPtr sad(sadp.pull());

        scoCache_->enableNamespace(nspace_,
                                   nspace_min,
                                   nspace_max,
                                   *sad);
    }
    else
    {
        LOG_INFO("Creating namespace" << nspace_);
        scoCache_->addNamespace(nspace_,
                                nspace_min,
                                nspace_max);
    }

    currentClusterLoc_ = ClusterLocation(lastSCOInBackend + 1);
    latestSCOnumberInBackend_ = lastSCOInBackend;
    scanSCOsForBackendRestart_(lastSCOInBackend);

    try
    {
        LOG_INFO("Creating new write SCO " << currentClusterLoc_);
        updateCurrentSCO_();
    }
    catch (TransientException &)
    {
        // Don't propagate the cache full exception
    }
}

DataStoreNG::~DataStoreNG()
{
    LOG_DEBUG("bye");

    WLOCK_DATASTORE();

    try
    {
        sync_();
    }
    catch (...)
    {
        LOG_ERROR("Failed to sync SCOs open for writing (ignoring)");
    }

    pendingTLogSCOs_.clear();
}

void DataStoreNG::validateConfig_()
{
    if (sco_mult_ == SCOMultiplier(0))
    {
        LOG_INFO("Invalid SCOMultiplier 0");
        throw fungi::IOException("invalid SCOMultiplier 0");
    }

    if (cluster_size_ == 0)
    {
        LOG_INFO("Invalid cluster size 0");
        throw fungi::IOException("invalid cluster size 0");
    }
}

void
DataStoreNG::pushSCO_(SCO sco)
{
    LOG_DEBUG("pushing SCO " << nspace_ << ": " << sco << " to backend");

    backend_task::WriteSCO *writeTask =
        new backend_task::WriteSCO(getVolume(),
                              this,
                              sco,
                              *currentCheckSum_,
                              OverwriteObject::T);
    VolManager::get()->scheduleTask(writeTask);
}

void
DataStoreNG::readFromSCO_(uint8_t* buf,
                          OpenSCOPtr osco,
                          size_t read_size,
                          size_t read_off)
{
    ssize_t res = osco->pread(buf,
                              read_size,
                              read_off);
    if (res != static_cast<ssize_t>(read_size))
    {
        LOG_ERROR("Read size " << res << " != requested size " << read_size);
        throw fungi::IOException("Read less than expected",
                                 osco->sco_ptr()->path().string().c_str(),
                                 EIO);
    }
}

void
DataStoreNG::touchCluster(const ClusterLocation &loc)
{
    RLOCK_DATASTORE();

    CachedSCOPtr sco_ptr(scoCache_->findSCO(nspace_,
                                            loc.sco()));
    if (sco_ptr)
    {
        scoCache_->signalSCOAccessed(sco_ptr, 1);
    }
}

namespace
{

struct PartialReadFallback
    : public backend::BackendConnectionInterface::PartialReadFallbackFun
{
    using Fun = std::function<CachedSCOPtr(SCO, bool& cached, InsistOnLatestVersion)>;
    Fun fun;
    std::vector<CachedSCOPtr> scoptrs;
    std::map<SCO, std::unique_ptr<youtils::FileDescriptor>> map;
    uint32_t hits;
    uint32_t misses;

    explicit PartialReadFallback(Fun&& f)
        : fun(std::move(f))
        , hits(0)
        , misses(0)
    {}

    virtual ~PartialReadFallback() = default;

    FileDescriptor&
    operator()(const backend::Namespace& /* nspace */,
               const std::string& object_name,
               InsistOnLatestVersion insist_on_latest)
    {
        const SCO sco(object_name);
        CachedSCOPtr scoptr;

        auto it = map.find(sco);
        if (it != map.end())
        {
            ++hits;
            return *it->second;
        }
        else
        {
            bool cached = false;
            scoptr = fun(sco,
                         cached,
                         insist_on_latest);
            scoptrs.push_back(scoptr);

            if (cached)
            {
                ++hits;
            }
            else
            {
                ++misses;
            }

            map[sco] = std::make_unique<yt::FileDescriptor>(scoptr->path(),
                                                            yt::FDMode::Read,
                                                            CreateIfNecessary::F);
            return *map.at(sco);
        }
    }
};

}

void
DataStoreNG::readClusters(const std::vector<ClusterReadDescriptor>& descs)
{
    RLOCK_DATASTORE();

    if(descs.empty())
    {
        return;
    }

    const size_t num_descs = descs.size();
    std::vector<std::string> clusterLocations;

    using PartialReadsMap =
        std::map<SCOCloneID, backend::BackendConnectionInterface::PartialReads>;
    PartialReadsMap partial_reads_map;

    for (size_t start = 0; start < num_descs; )
    {
        size_t num_clusters = 1;
        const ClusterLocation& prev = descs[start].getClusterLocation();
        SCOCloneID start_cid = prev.cloneID();

        for (size_t i = start + 1; i < num_descs; ++i)
        {
            // Can be optimized by comparing against the first clusterloc
            // const ClusterLocation& prev = descs[i - 1].getClusterLocation();
            const ClusterLocation& cur = descs[i].getClusterLocation();
            if (prev.number() == cur.number() and
                prev.version() == cur.version() and
                start_cid == cur.cloneID() and
                prev.offset() + (i-start) == cur.offset() and
                // use preadv and get rid of that:
                descs[i - 1].getBuffer() + getClusterSize() == descs[i].getBuffer())
            {
                ++num_clusters;
            }
            else
            {
                break;
            }
        }

        bool hit = read_adjacent_clusters_(descs[start],
                                           num_clusters,
                                           false);
        if (not hit)
        {
            SCO sco = descs[start].getClusterLocation().sco();

            if (prev.cloneID() == SCOCloneID(0) and
                prev.number() > latestSCOnumberInBackend_ and
                pendingTLogSCOs_.find(sco) == pendingTLogSCOs_.end())
            {
                // fetch from the FOC if present
                hit = read_adjacent_clusters_(descs[start],
                                              num_clusters,
                                              true);
                VERIFY(hit);
            }
            else
            {
                // the SCO is (supposed to be) on the backend
                sco.cloneID(SCOCloneID(0));

                partial_reads_map[start_cid].emplace_back(sco.str());
                auto& partial_read = partial_reads_map[start_cid].back();
                partial_read.buf = descs[start].getBuffer();
                partial_read.offset =
                    descs[start].getClusterLocation().offset() * getClusterSize();
                partial_read.size = num_clusters * getClusterSize();
            }
        }

        start += num_clusters;
    }

    const InsistOnLatestVersion insist_on_latest =
        VolManager::get()->allow_inconsistent_partial_reads.value() ?
        InsistOnLatestVersion::F :
        InsistOnLatestVersion::T;

    // Candidate for parallelization
    for (const auto& partial_reads : partial_reads_map)
    {
        yt::SteadyTimer t;

        const SCOCloneID cid = partial_reads.first;
        auto& bi = getVolume()->getBackendInterface(cid);
        auto fun([&](SCO sco,
                     bool& cached,
                     InsistOnLatestVersion) -> CachedSCOPtr
                 {
                     sco.cloneID(cid);
                     return getSCO_(sco,
                                    bi->clone(),
                                    cached,
                                    nullptr);
                 });

        PartialReadFallback fallback(fun);

        try
        {
            bi->partial_read(partial_reads.second,
                             fallback,
                             insist_on_latest);
        }
        catch (be::BackendConnectFailureException&)
        {
            throw TransientException("Backend connection failure");
        }

        cacheHitCounter_ += fallback.hits;
        cacheMissCounter_ += fallback.misses;

        if (fallback.misses)
        {
            const auto duration_us(bc::duration_cast<bc::microseconds>(t.elapsed()));
            PerformanceCounters& c = getVolume()->performance_counters();
            c.backend_read_request_usecs.count(duration_us.count());

            uint64_t bytes = 0;
            for (const auto& pr : partial_reads.second)
            {
                bytes += pr.size;
            }

            c.backend_read_request_size.count(bytes);
        }
    }
}

bool
DataStoreNG::read_adjacent_clusters_(const ClusterReadDescriptor& desc,
                                     size_t num_clusters,
                                     bool fetch_if_necessary)
{
    uint8_t* buf = desc.getBuffer();

    const ClusterLocation& loc = desc.getClusterLocation();
    SCO sconame = loc.sco();


    LOG_DEBUG(loc);

    OpenSCOPtr osco = openSCOs_.find(sconame);
    if (osco)
    {
        LOG_TRACE("using cached open sco " << nspace_ << ": " << loc);

        cacheHitCounter_ += num_clusters;
    }
    else
    {
        bool cached = false;

        CachedSCOPtr sco = fetch_if_necessary ?
            getSCO_(sconame,
                    desc.getBackendInterface(),
                    cached,
                    nullptr) :
            scoCache_->findSCO(nspace_,
                               sconame);

        if (not sco)
        {
            cacheMissCounter_ += num_clusters;
            return false;
        }

        if (not fetch_if_necessary)
        {
            cacheHitCounter_ += num_clusters;
            cached = true;
        }
        else if (cached)
        {
            cacheHitCounter_ += num_clusters;
        }
        else
        {
            ++cacheMissCounter_;
            uint64_t hits = num_clusters - 1;
            cacheHitCounter_ += hits;
        }

        try
        {
            osco = sco->open(FDMode::Read);
        }
        catch (std::exception& e)
        {
            LOG_ERROR(nspace_ << ": failed to open " << sco->path() << ": " <<
                      e.what());
            reportIOError_(sco, true, e.what());
        }
        catch (...)
        {
            LOG_ERROR(nspace_ << ": failed to open " << sco->path() <<
                      ": unknown exception");
            reportIOError_(sco, true, "unknown exception");
        }

        VERIFY(osco != 0);
    }

    try
    {
        readFromSCO_(buf,
                     osco,
                     num_clusters * cluster_size_,
                     loc.offset() * cluster_size_);

        osco->sco_ptr()->incRefCount(num_clusters);

        return true;
    }
    catch (std::exception& e)
    {
        LOG_ERROR(nspace_ << ": failed to read " << num_clusters <<
                  " clusters " << loc << ": " << e.what());
        reportIOError_(osco->sco_ptr(), true, e.what());
    }
    catch (...)
    {
        LOG_ERROR(nspace_ << ": failed to read " << num_clusters <<
                  " cluster " << loc << ": unknown exception");
        reportIOError_(osco->sco_ptr(), true, "unknown exception");
    }

    UNREACHABLE;
}

void
DataStoreNG::updateCurrentSCO_()
{
    tracepoint(openvstorage_volumedriver,
               new_sco,
               nspace_.str().c_str(),
               currentClusterLoc_.sco().number());

    LOG_DEBUG("creating new write SCO " << nspace_ << ": " <<
              currentClusterLoc_);

    currentCheckSum_.reset(new CheckSum());

    SCO sco = currentClusterLoc_.sco();

    openSCOs_.erase_bucket(sco);

    CachedSCOPtr scoptr;
    try
    {
        scoptr = scoCache_->createSCO(nspace_,
                                      sco,
                                      sco_mult_.t * cluster_size_);
    }
    catch (SCOCacheNoMountPointsException& e)
    {
        LOG_FATAL(nspace_ << ": " << e.what());
        getVolume()->halt();
        throw;
    }

    LOG_INFO("created new write SCO " << currentClusterLoc_.sco()
             << " for Volume " << nspace_);

    // Report an I/O error if opening fails. this will lead to the sco that was
    // just created being removed again from the scocache, the mountpoint being
    // offlined and a TransientException being raised.
    try
    {
        openSCOs_.insert(scoptr->open(FDMode::ReadWrite));
    }
    catch (std::exception& e)
    {
        reportIOError_(scoptr, false, e.what());
    }
    catch (...)
    {
        reportIOError_(scoptr, false, "unknown exception");
    }
    VERIFY(currentSCO_() != 0);
    //    syncCurrentCheckSum_();
}

MaybeCheckSum
DataStoreNG::pushAndUpdateCurrentSCO_(bool ignore_transient_errors)
{
    LOG_DEBUG("Pushing sco " << currentSCO_()->sco_ptr()->getSCO() << "to the backend");

    VERIFY(currentSCO_() != 0);

    pushSCO_(currentSCO_()->sco_name());

    ClusterLocation loc(currentClusterLoc_.number() + 1,
                        0,
                        currentClusterLoc_.cloneID());
    currentClusterLoc_ = loc;

    VERIFY(currentCheckSum_.get());
    MaybeCheckSum cs(*currentCheckSum_);

    try
    {
        updateCurrentSCO_();
        return cs;
    }
    catch (TransientException &)
    {
        if (not ignore_transient_errors)
        {
            throw;
        }
        else
        {
            return cs;
        }
    }
}

void
DataStoreNG::writeClusterToLocation(const uint8_t* buf,
                                    const ClusterLocation& loc,
                                    uint32_t& throttle)
{
    WLOCK_DATASTORE();
    LOG_DEBUG(nspace_ << ": forced write to " << loc << ", current loc: " <<
              currentClusterLoc_);

    VERIFY(loc.cloneID() == 0);
    VERIFY(loc.version() == 0);

    if (loc != currentClusterLoc_)
    {
        LOG_ERROR(nspace_ << ": received invalid clusterlocation: current loc " <<
                  currentClusterLoc_ << ", got " << loc);
        throw fungi::IOException("got unexpected clusterlocation",
                                 nspace_.c_str());
    }

    std::vector<ClusterLocation> dummy(1);
    writeClusters_(buf, dummy, 1, throttle);
    VERIFY(dummy[0] == loc);
}

void
DataStoreNG::writeClusters(const uint8_t* buf,
                           std::vector<ClusterLocation>& locs,
                           size_t num_locs,
                           uint32_t& throttle_usecs)
{
    WLOCK_DATASTORE();
    writeClusters_(buf,
                   locs,
                   num_locs,
                   throttle_usecs);
}

void
DataStoreNG::writeClusters_(const uint8_t* buf,
                            std::vector<ClusterLocation>& locs,
                            size_t num_locs,
                            uint32_t& throttle_usecs)
{
    ASSERT_WLOCKED();
    LOG_DEBUG("CurrentClusterLoc before write: " << currentClusterLoc_);

    if (currentSCO_() == 0)
    {
        LOG_DEBUG("current SCO == 0 - previous cache full event?");
        updateCurrentSCO_();
    }

    VERIFY(currentSCO_() != 0);

    uint64_t wsize = num_locs * cluster_size_;
    try
    {
        off_t off = currentClusterLoc_.offset() * cluster_size_;
        ssize_t res = currentSCO_()->pwrite(buf,
                                            wsize,
                                            off,
                                            throttle_usecs);
        if (res != static_cast<ssize_t>(wsize))
        {
            LOG_ERROR("Written size " << res << " != expected size " <<
                      wsize);
            throw fungi::IOException("Wrote less than expected",
                                     currentSCO_()->sco_ptr()->path().string().c_str(),
                                     EIO);
        }
    }
    catch (std::exception& e)
    {
        LOG_ERROR(nspace_ << ": failed to write " << num_locs <<
                  " clusters starting at " << currentClusterLoc_ << ": " <<
                  e.what());
        reportIOError_(currentSCO_()->sco_ptr(), false, e.what());
    }
    catch (...)
    {
        LOG_ERROR(nspace_ << ": failed to write " << num_locs <<
                  " clusters starting at " << currentClusterLoc_ <<
                  ": unkown exception");
        reportIOError_(currentSCO_()->sco_ptr(), false, "unknown exception");
    }

    currentCheckSum_->update(buf, wsize);

    for (size_t i = 0; i < num_locs; ++i)
    {
        locs[i] = ClusterLocation(currentClusterLoc_.number(),
                                  currentClusterLoc_.offset() + i);
    }

    // Y42 Update current cluster loc offset?
    currentClusterLoc_.offset(currentClusterLoc_.offset() + num_locs);
    LOG_DEBUG("CurrentClusterLoc after write: " << currentClusterLoc_);
}

MaybeCheckSum
DataStoreNG::sync_()
{
    // This needs to be redone:
    // currently the openSCOs_ are protected here by the rwlock held exclusively,
    // so another thread cannot modify openSCOs_.
    // OTOH, concurrent readers are protected from modifying openSCOs (e.g. one
    // tries to read while the other one reports an error) by a spinlock inside
    // openSCOs_.
    // This is obviously far too convoluted/brittle.
    for (WriteSCOCache::iterator it = openSCOs_.begin();
         it != openSCOs_.end();
         ++it)
    {
        if (*it != 0)
        {
            try
            {
                (*it)->sync();
                if (*it != currentSCO_())
                {
                    *it = 0;
                }
            }
            catch (std::exception& e)
            {
                reportIOError_((*it)->sco_ptr(), false, e.what());
            }
            catch (...)
            {
                reportIOError_((*it)->sco_ptr(), false, "unknown exception");
            }
        }
    }
    // Check whether Checksum is always reset correctly
    if (currentClusterLoc_.offset() > 0)
    {
        VERIFY(currentCheckSum_.get() != 0);
        return MaybeCheckSum(*currentCheckSum_);
    }
    else
    {
        return MaybeCheckSum();
    }
}

MaybeCheckSum
DataStoreNG::sync()
{
    WLOCK_DATASTORE();
    return sync_();
}

void
DataStoreNG::writtenToBackend(SCO sconame)
{
    WLOCK_DATASTORE();

    LOG_DEBUG("SCO " << nspace_ << ": " << sconame << "finished");

    pendingTLogSCOs_.insert(sconame);
}

// Don't let any exceptions escape since the sco is safely in the backend.
// Just reportIOError_ (which will abandon the mountpoint), and on next access
// to the sco it can be fetched from the backend or the failovercache.
void
DataStoreNG::scoWrittenToBackend_(SCO sconame)
{
    tracepoint(openvstorage_volumedriver,
               mark_sco_disposable_start,
               nspace_.str().c_str(),
               sconame.number());

    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         tracepoint(openvstorage_volumedriver,
                                                    mark_sco_disposable_end,
                                                    nspace_.str().c_str(),
                                                    sconame.number(),
                                                    std::uncaught_exception());
                                     }));

    LOG_DEBUG(nspace_ << ": SCO " << sconame);
    CachedSCOPtr sco = nullptr;

    try
    {
        OpenSCOPtr osco = openSCOs_.find(sconame);
        if (osco != 0)
        {
            sco = osco->sco_ptr();
            openSCOs_.erase(sconame);
        }
        else
        {
            sco = scoCache_->findSCO(nspace_,
                                     sconame);
        }

        if (sco)
        {
            scoCache_->setSCODisposable(sco);
        }
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR(nspace_ << ": cannot set SCO " <<
                      sconame << " disposable: " << EWHAT);
            if (sco != nullptr)
            {
                try
                {
                    reportIOError_(sco,
                                   false,
                                   EWHAT);
                }
                catch (...)
                {
                    // swallow
                }
            }
            else
            {
                LOG_ERROR(nspace_ << ": SCO " << sconame <<
                          " not found - not reporting as an I/O error");
            }
        });
}

// we could do away with writtenToBackend and pendingTLogSCOs_ entirely if SCONames
// are always consecutive (which they should be ATM)
void
DataStoreNG::writtenToBackendUpTo(SCO sconame)
{
    LOG_DEBUG(nspace_ << ": latest SCO in backend: " << sconame);

    std::vector<SCO> pending;

    {
        WLOCK_DATASTORE();

        latestSCOnumberInBackend_ = sconame.number();
        pending.reserve(pendingTLogSCOs_.size());

        auto it = pendingTLogSCOs_.begin();
        while (it != pendingTLogSCOs_.end())
        {
            if (it->number() <= latestSCOnumberInBackend_)
            {
                pending.push_back(*it);
                it = pendingTLogSCOs_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    for (const auto& sco : pending)
    {
        WLOCK_DATASTORE();
        scoWrittenToBackend_(sco);
    }
}

MaybeCheckSum
DataStoreNG::finalizeCurrentSCO()
{
    WLOCK_DATASTORE();

    LOG_DEBUG("SCO " << nspace_ << ": " << currentClusterLoc_);

    if (currentClusterLoc_.offset() != 0)
    {
        return pushAndUpdateCurrentSCO_();
    }
    else
    {
        return MaybeCheckSum();
    }
}


uint64_t DataStoreNG::getClusterSize() const
{
    return cluster_size_;
}

// Z42 TODO: - re-consider exception handling!
void DataStoreNG::destroy(const DeleteLocalData delete_local_data)
{
    WLOCK_DATASTORE();

    LOG_INFO(getVolume()->getName() << ": destroying DataStore, " << delete_local_data);

    sync_();

    if (currentSCO_() != 0)
    {
        SCO scoName = currentSCO_()->sco_name();
        VERIFY(openSCOs_.find(scoName) == currentSCO_());
        openSCOs_.erase(scoName);
    }

    if (T(delete_local_data))
    {
        //        checkSumStore_.destroy();
        scoCache_->removeNamespace(nspace_);
    }
    else
    {
        //        checkSumStore_.sync();
        scoCache_->disableNamespace(nspace_);
    }
}

void
DataStoreNG::removeSCO(SCO scoName,
                       bool removeNonDisposable)
{
    WLOCK_DATASTORE();

    // Z42: disallow removing the currentSCO?
    removeSCO_(scoName, removeNonDisposable);
}

void
DataStoreNG::removeSCO_(SCO scoName,
                        bool removeNonDisposable,
                        bool updateCurrentLOCandSCO)
{
    CachedSCOPtr sco(scoCache_->findSCO(nspace_,
                                        scoName));
    if (not sco)
    {
        LOG_INFO(nspace_ << ": SCO " << scoName << " not found, ignoring");
        return;
    }

    bool disposable = scoCache_->isSCODisposable(sco);
    scoCache_->removeSCO(nspace_,
                         scoName,
                         removeNonDisposable);

    if (not disposable and removeNonDisposable)
    {
        if (currentSCO_() != 0 and sco == currentSCO_()->sco_ptr())
        {
            openSCOs_.erase(sco->getSCO(), false);

            if (updateCurrentLOCandSCO)
            {
                // the currentSCO is expected to be in the openSCOs_, so it's
                // closed by now.
                ClusterLocation loc(currentClusterLoc_.number() + 1,
                                    0,
                                    currentClusterLoc_.cloneID());
                currentClusterLoc_ = loc;

                try
                {
                    updateCurrentSCO_();
                }
                catch (TransientException &)
                {
                    // don't propagate - on the next write (once there is enough cache
                    // space) a new SCO will be created
                }
            }
        }
        else
        {
            openSCOs_.erase(sco->getSCO(), false);
        }
        // the checksum *must* be there - so let exceptions escape.
        // checkSumStore_.erase(scoName);
    }
}

uint64_t
DataStoreNG::getWriteUsed()
{
    SCONameList lst;
    scoCache_->getSCONameList(nspace_, lst, false);
    uint64_t res = 0;

    for (const auto& sconame : lst)
    {
        CachedSCOPtr sco(scoCache_->findSCO(nspace_,
                                            sconame));
        if (sco)
        {
            if (currentSCO_() != 0 and sco == currentSCO_()->sco_ptr())
            {
                res += currentClusterLoc_.offset() * cluster_size_;
            }
            else
            {
                res += sco->getSize();
            }
        }
    }

    return res;
}

uint64_t
DataStoreNG::getReadUsed()
{
    SCONameList lst;
    scoCache_->getSCONameList(nspace_, lst, true);
    uint64_t res = 0;

    for (const auto& sconame : lst)
    {
        CachedSCOPtr sco(scoCache_->findSCO(nspace_,
                                            sconame));
        if (sco)
        {
            res += sco->getSize();
        }
    }

    return res;
}

void DataStoreNG::listSCOs(SCONameList &list, bool disposable)
{
    WLOCK_DATASTORE();

    if (scoCache_->hasNamespace(nspace_))
    {
        scoCache_->getSCONameList(nspace_, list, disposable);
    }
}

CachedSCOPtr
DataStoreNG::getSCO(SCO scoName,
                    BackendInterfacePtr bi,
                    const CheckSum* cs)
{
    WLOCK_DATASTORE();
    bool cached(false);
    return getSCO_(scoName, std::move(bi), cached, cs);
}

void
DataStoreNG::getAndPushSCOForLocalRestart(SCO sco,
                                          const CheckSum& cs)
{
    WLOCK_DATASTORE();

    VERIFY(sco.cloneID() == 0);
    VERIFY(sco.number() > latestSCOnumberInBackend_);

    CachedSCOPtr scoptr;
    std::unique_ptr<SCOFetcher> fetcher(new FailOverCacheSCOFetcher(sco,
                                                                    &cs,
                                                                    getVolume()));

    bool gotit = false;
    while (!gotit)
    {
        try
        {

            scoptr = scoCache_->getSCO(nspace_,
                                       sco,
                                       sco_mult_.t * cluster_size_,
                                       *fetcher,
                                       0);

            gotit = true;
        }
        catch (TransientException& e)
        {
            LOG_WARN("failed to get get " << nspace_ << "/"
                     << sco << ": " << e.what() << " - retrying");
            boost::this_thread::sleep(boost::posix_time::milliseconds(500));
        }
    }

    backend_task::WriteSCO *writeTask =
        new backend_task::WriteSCO(getVolume(),
                              this,
                              sco,
                              cs,
                              OverwriteObject::T);
    VolManager::get()->scheduleTask(writeTask);}

CachedSCOPtr
DataStoreNG::getSCO_(SCO sco,
                     BackendInterfacePtr bi,
                     bool& cached,
                     const CheckSum* cs)
{
    std::unique_ptr<SCOFetcher> fetcher;
    ClusterLocation loc(sco, 0);

    ASSERT_RLOCKED();

    if (loc.cloneID() != SCOCloneID(0) ||
        loc.number() <= latestSCOnumberInBackend_ ||
        pendingTLogSCOs_.find(sco) != pendingTLogSCOs_.end())
    {
        VERIFY(bi);
        // We should not get here in the read path anymore
        fetcher.reset(new BackendSCOFetcher(sco, getVolume(), bi->clone()));
    }
    else
    {
        fetcher.reset(new FailOverCacheSCOFetcher(sco,
                                                  cs,
                                                  getVolume()));
    }

    try
    {
        return scoCache_->getSCO(nspace_,
                                 sco,
                                 sco_mult_.t * cluster_size_,
                                 *fetcher,
                                 &cached);
    }
    catch (SCOCacheNoMountPointsException& e)
    {
        LOG_FATAL(nspace_ << ": " << e.what());
        getVolume()->halt();
        throw;
    }
}

void
DataStoreNG::reportIOError(SCO sconame, const char* errmsg)
{
    WLOCK_DATASTORE();

    try
    {
        CachedSCOPtr sco = scoCache_->findSCO_throw(nspace_,
                                                    sconame);
        reportIOError_(sco, true, errmsg);
    }
    catch (TransientException&)
    {
        // swallow it
    }
}

// never returns, *always* throws a TransientException
void
DataStoreNG::reportIOError_(CachedSCOPtr sco, bool read, const char* errmsg)
{
    SERIALIZE_ERROR_HANDLING();

    // always persist the current SCO's checksum to the checksumstore:
    // - currentCheckSum_ is only updated if data made it to the SCO
    //   (not necessarily to persistent storage though!)
    // - we need to handle this race:
    //   (1) create new sco -> checksum = ~0x0
    //   (2) write cluster 1 -> ok, data in page cache and on foc ->
    //       checksum != ~0x0
    //   (3) period of inactivity, async writeout of current sco -> error
    //   (4) read cluster 1 -> try to fetch from foc, lookup checksum in
    //       checksumstore -> checksum mismatch!
    LOG_INFO("SCO " << sco->path() << ", error " << errmsg);

    if (currentSCO_() != 0)
    {
        //syncCurrentCheckSum_();
    }

    if (read)
    {
        VolumeDriverError::report(scoCache_->isSCODisposable(sco) ?
                                  events::VolumeDriverErrorCode::ReadFromDisposableSCO :
                                  events::VolumeDriverErrorCode::ReadFromNonDisposableSCO,
                                  errmsg,
                                  getVolume()->getName());
    }
    else
    {
        VolumeDriverError::report(events::VolumeDriverErrorCode::WriteToSCO,
                                  errmsg,
                                  getVolume()->getName());
    }

    scoCache_->reportIOError(sco);
    const SCO sconame = sco->getSCO();

    openSCOs_.erase(sconame, false);

    // - current sco: try to refetch it, verify the checksum and continue (checksum
    //   matches) or halt the volume (checksum mismatch)
    // - nondisposable sco: next access - either by a reader or by the write task
    //                in the queue - will fetch it again from the failovercache
    // - disposable sco: next access by a reader will fetch it again from the
    //                backend
    if (currentClusterLoc_.sco() == sconame)
    {
        if (currentClusterLoc_.offset() != 0)
        {
            bool cached(false);
            CachedSCOPtr sco_ptr(getSCO_(sconame, 0, cached, 0));
            openSCOs_.insert(sco_ptr->open(FDMode::ReadWrite));
        }
    }

    throw TransientException("Retryable I/O error");
}

OpenSCOPtr
DataStoreNG::currentSCO_() const
{
    return openSCOs_.find(currentClusterLoc_.sco());
}

ssize_t
DataStoreNG::getRemainingSCOCapacity() const
{
    RLOCK_DATASTORE();
    ssize_t ret = sco_mult_.t - currentClusterLoc_.offset();

    return ret;
}

void
DataStoreNG::setSCOMultiplier(const SCOMultiplier sco_mult)
{
    VERIFY(sco_mult != SCOMultiplier(0));

    WLOCK_DATASTORE();
    sco_mult_ = sco_mult;
}

SCOMultiplier
DataStoreNG::getSCOMultiplier() const
{
    RLOCK_DATASTORE();
    return sco_mult_;
}

}

// Local Variables: **
// mode: c++ **
// End: **
