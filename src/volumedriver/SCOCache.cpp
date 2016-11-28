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

#include "ClusterLocation.h"
#include "SCOAccessData.h"
#include "SCOCache.h"
#include "SCOCacheMountPoint.h"
#include "SCOCacheNamespace.h"
#include "TransientException.h"
#include "VolumeDriverError.h"

#include <fstream>
#include <set>
#include <limits>
#include <boost/scope_exit.hpp>

#include <boost/thread/lock_guard.hpp>

#include <youtils/Assert.h>
#include <youtils/Catchers.h>
#include <youtils/IOException.h>

#define LOCK_CLEANUP()                                  \
    cleanup_lock_type::scoped_lock __cl(cleanupLock_)

#define LOCK_XVALS()                                                    \
    boost::lock_guard<decltype(xValSpinLock_)> __sl(xValSpinLock_)

#define WLOCK_CACHE()                           \
    fungi::ScopedWriteLock __w(rwLock_)

#define RLOCK_CACHE()                           \
    fungi::ScopedReadLock __r(rwLock_)

#define LOCK_NSPACE_MGMT()                                         \
    boost::lock_guard<nspace_mgmt_lock_type> __nmlg(nspaceMgmtLock_)

#ifndef NDEBUG

#define ASSERT_RWLOCKED()                       \
    rwLock_.assertLocked()

#define ASSERT_SPINLOCKED()                     \
    xvalSpinLock_.assertLocked()

#define ASSERT_CLEANUP_LOCKED()                 \
    assert(cleanupLock_.try_lock() == false)

#define ASSERT_NSPACE_MGMT_LOCKED()        \
    assert(nspaceMgmtLock_.try_lock() == false)

#else

#define ASSERT_RWLOCKED()                       \
    do {} while (false)

#define ASSERT_SPINLOCKED()                     \
    do {} while (false)

#define ASSERT_CLEANUP_LOCKED()                 \
    do {} while (false)

#define ASSERT_NSPACE_MGMT_LOCKED()        \
    do {} while (false)

#endif

namespace volumedriver
{

namespace fs = boost::filesystem;
namespace bi = boost::intrusive;
using namespace initialized_params;

SCOCache::SCOCache(const boost::property_tree::ptree& pt)
    : VolumeDriverComponent(RegisterComponent::T,
                            pt)
    , cleanupLock_()
    , nspaceMgmtLock_()
    , rwLock_("SCOCacheRWLock")
    , xValSpinLock_()
    , currentMountPoint_(mountPoints_.end())
    , cachedXValMin_(0)
    , initialXVal_(1.0)
    , mpErrorCount_(0)
    , trigger_gap(pt)
    , backoff_gap(pt)
    , discount_factor(pt)
    , scocache_mount_points(pt)
    , datastore_throttle_usecs(pt)
{

    ConfigurationReport rep;

    if(not checkParameterConfig(pt,rep))
    {
        for (auto confprob: rep)
        {
            LOG_FATAL("Configuration problem " << confprob.problem());
        }

        throw fungi::IOException("Configuration problem ");
    }


    LOCK_CLEANUP();
    WLOCK_CACHE();
    try
    {
        initMountPoints_();
    }
    catch (...)
    {
        destroy_();
        throw;
    }

    LOG_DEBUG("initialised");
}

SCOCache::~SCOCache()
{
    LOCK_CLEANUP();
    WLOCK_CACHE();
    LOG_DEBUG("bye");
    destroy_();
    LOG_DEBUG("Exiting SCOCache::~SCOCache");

}

namespace
{

class MountPointOfflinedChecker
{
public:
    MountPointOfflinedChecker(uint64_t errcount)
        : errcount_(errcount)
    {}

    bool
    operator()(const SCOCacheMountPointPtr mp)
    {
        return doit_(mp);
    }

private:
    DECLARE_LOGGER("MountPointOfflinedCheck");

    const uint64_t errcount_;

    bool
    doit_(const SCOCacheMountPointPtr mp)
    {
        uint64_t mperrcount;

        try
        {
            mperrcount = mp->getErrorCount();
        }
        catch (std::exception& e)
        {
            LOG_ERROR(mp->getPath() << ": failed to get error count: " <<
                      e.what() << " - dropping it");
            return true;
        }
        catch (...)
        {
            LOG_ERROR(mp->getPath() <<
                      ": failed to get error count: unknown exception - dropping it");
            return true;
        }

        if (mperrcount != errcount_)
        {
            LOG_ERROR(mp->getPath() << " was previously offlined, dropping it");
            return true;
        }
        else
        {
            return false;
        }
    }
};

struct NewMountPointInitialiser
{
    explicit NewMountPointInitialiser(uint64_t errcount)
        : errcount_(errcount)
    {}

    bool
    operator()(SCOCacheMountPointPtr mp)
    {
        try
        {
            mp->newMountPointStage2(errcount_);
            return false;
        }
        catch (std::exception& e)
        {
            LOG_ERROR("Failed to initialize new mountpoint " << mp->getPath() <<
                      ": " << e.what() << " - dropping it");
            return true;
        }
    }

    DECLARE_LOGGER("InitialiseNewMountPoint");
    const uint64_t errcount_;
};

struct MountPointCompare
{
    bool
    operator()(const SCOCacheMountPointPtr mp1, const SCOCacheMountPointPtr mp2)
    {
        return mp1->uuid() < mp2->uuid();
    }
};

struct MountPointDupChecker
{
    bool
    operator()(const SCOCacheMountPointPtr mp1, const SCOCacheMountPointPtr mp2)
    {
        if (mp1->uuid() == mp2->uuid())
        {
            LOG_ERROR("Duplicate mountpoints " << mp1->getPath() << ", " <<
                      mp2->getPath() << " - dropping " << mp2->getPath());
            return true;
        }
        else
        {
            return false;
        }
    }
    DECLARE_LOGGER("DuplicateMountPointCheck");
};

}

void
SCOCache::initMountPoints_()
{
    ASSERT_RWLOCKED();

    SCOCacheMountPointList newOnes;

    for (const MountPointConfig& cfg: scocache_mount_points.value())
    {
        try
        {
            // mountpoint path must exist
            // mountpoint restart: lockfile must exist, mp can be nonempty
            // new mountpoint: lockfile must not exist, mp must be empty
            bool restart = SCOCacheMountPoint::exists(cfg);
            SCOCacheMountPointPtr mp(new SCOCacheMountPoint(*this,
                                                            cfg,
                                                            restart));
            if (restart)
            {
                mpErrorCount_ = std::max(mpErrorCount_, mp->getErrorCount());
                mountPoints_.push_back(mp);
            }
            else
            {
                newOnes.push_back(mp);
            }
        }
        catch (std::exception& e)
        {
            LOG_ERROR("failed to initialize mountpoint " << cfg.path << ": " <<
                      e.what() << " - proceeding without this mountpoint");
        }
    }

    // init new mountpoints:
    {
        NewMountPointInitialiser init(mpErrorCount_);
        newOnes.remove_if(init);
    }

    mountPoints_.splice(mountPoints_.end(), newOnes);

    // drop duplicates:
    {
        MountPointCompare cmp;
        mountPoints_.sort(cmp);

        MountPointDupChecker chk;
        mountPoints_.unique(chk);
    }

    // drop previously offlined mountpoints:
    {
        MountPointOfflinedChecker chk(mpErrorCount_);
        mountPoints_.remove_if(chk);
    }

    currentMountPoint_ = mountPoints_.begin();

    // Increase the error count on each startup to avoid old mountpoints
    // being re-added on later restarts (this newly introduced feature clashes with
    // the "mpErrorCount_" name ... oh well).
    bumpMountPointErrorCount_();

    if (mountPoints_.size() == 0)
    {
        LOG_FATAL("no usable mountpoints available, giving up");
        throw fungi::IOException("no sco cache mountpoints available");
    }
}

void
SCOCache::destroy_()
{
    ASSERT_CLEANUP_LOCKED();
    ASSERT_RWLOCKED();

    LOG_DEBUG("Clearing the mountpoints");

    mountPoints_.clear();

    for (NSMap::value_type &p: nsMap_)
    {
        LOG_DEBUG("deleting ");
        delete p.second;
    }
}

void
SCOCache::initXVals_(SCOCacheNamespace* ns, const SCOAccessData& sad)
{
    ASSERT_RWLOCKED();

    // 1. feed sco access data to the SCOs - the ones found are unblocked.
    const SCOAccessData::VectorType& sadv = sad.getVector();
    for (SCOAccessData::VectorType::const_iterator sadIt = sadv.begin();
         sadIt != sadv.end();
         ++sadIt)
    {
        SCOCacheNamespaceEntry* e = ns->findEntry(sadIt->first);
        if (e != nullptr)
        {
            float xval = sadIt->second;
            CachedSCOPtr sco = e->getSCO();
            sco->setXVal(xval);
            e->setBlocked(false);
        }
    }

    // 2. now deal with the SCOs that are still left blocked
    // Z42: this used to use the avg (sum / count)

    float xValAvg = getInitialXVal_();

    for (SCOCacheNamespace::value_type& t: *ns)
    {
        SCOCacheNamespaceEntry& e = t.second;
        CachedSCOPtr sco = e.getSCO();

        if (e.isBlocked())
        {
            sco->setXVal(xValAvg);
            e.setBlocked(false);
        }
    }

    rescaleXVals_();
}

void
SCOCache::insertSCO_(CachedSCOPtr sco,
                     bool blocked)
{
    NSMap::iterator it = nsMap_.find(sco->getNamespace()->getName());
    VERIFY(it != nsMap_.end());

    SCOCacheNamespace* ns = it->second;

    SCOCacheNamespaceEntry e(sco, blocked);
    std::pair<SCOCacheNamespace::iterator, bool> res =
        ns->insert(std::make_pair(sco->getSCO(), e));

    // if(not res.second)
    // {
    //     LOG_FATAL("WAS CHANGED BY IMMANUEL TO INVESTIGATE");
    // }

    VERIFY(res.second == true);
}

void
SCOCache::removeSCO_(CachedSCOPtr sco,
                     bool unlink)
{
    ASSERT_RWLOCKED();

    SCOCacheNamespace* ns = findNamespace_throw_(sco->getNamespace()->getName());
    ns->erase(sco->getSCO());

    if (unlink)
    {
        try
        {
            sco->remove();
        }
        catch (std::exception& e)
        {
            LOG_ERROR("Failed to remove sco " << sco->path() << ": " << e.what());
            reportIOError_(sco);
            throw;
        }
    }
}

void
SCOCache::findAndRemoveSCO_(const backend::Namespace& nsname,
                            SCO scoName,
                            bool removeNonDisposable,
                            bool mustBeBlocked,
                            bool unlink)
{
    ASSERT_RWLOCKED();

    SCOCacheNamespace* ns = findNamespace_throw_(nsname);
    SCOCacheNamespaceEntry* e = ns->findEntry_throw(scoName);
    if (not mustBeBlocked && e->isBlocked())
    {
        LOG_INFO("Not removing SCO " << scoName <<
                 " from namespace " << nsname <<
                 " as its currently being retrieved from the backend. " <<
                 " The cleaner will reap it later.");
        throw TransientException("Not removing a SCO that's concurrently being retrieved");
    }

    CachedSCOPtr sco = e->getSCO();

    if (!sco->isDisposable() && !removeNonDisposable)
    {
        std::stringstream ss;
        ss << nsname << "/" << scoName;
        LOG_ERROR("Attempt to remove non-disposable SCO " << ss.str());
        throw fungi::IOException("attempt to remove non-disposable SCO",
                                 ss.str().c_str(),
                                 EBUSY);
    }

    // we could avoid the second lookup by fetching an iterator above
    removeSCO_(sco, unlink);
}

void
SCOCache::insertScannedSCO(CachedSCOPtr sco)
{
    ASSERT_RWLOCKED();

    insertSCO_(sco, true);
}


// currentMountPoint_ must never point to mountPoints_.end() - if it does
// we've offlined the last mountpoint and cannot do any more I/O
SCOCacheMountPointPtr
SCOCache::getWriteMountPoint_(uint64_t scoSize)
{
    ASSERT_RWLOCKED();

    if (currentMountPoint_ == mountPoints_.end())
    {
        throw SCOCacheNoMountPointsException();
    }

    SCOCacheMountPointList::const_iterator it = currentMountPoint_;
    ++it;

    // 2 passes:
    // (1) avoids choking mountpoints
    // (2) if all are choking, check the available size

    while (currentMountPoint_ != it)
    {
        if (it == mountPoints_.end())
        {
            it = mountPoints_.begin();
            continue;
        }

        if ((*it)->isChoking())
        {
            ++it;
            continue;
        }
        else
        {
            break;
        }
    }

    if (it == currentMountPoint_ && (*it)->isChoking())
    {
        ++it;
        while (currentMountPoint_ != it)
        {
            if (it == mountPoints_.end())
            {
                it = mountPoints_.begin();
                continue;
            }

            // We need this check for now, since if there's not enough space in the
            // filesystem we might be able to create a SCO but will get an
            // I/O error while writing to it. That needs to be fixed first before
            // this check can be removed.
            if ((*it)->getUsedSize() + scoSize > (*it)->getCapacity())
            {
                ++it;
                continue;
            }
            else
            {
                break;
            }
        }
    }

    if ((*it)->getUsedSize() + scoSize > (*it)->getCapacity())
    {
        throw TransientException("cache full");
    }

    currentMountPoint_ = it;
    return *it;
}

void
SCOCache::signalSCOAccessed(CachedSCOPtr sco, uint32_t num)
{
    //TODO probably make non-blocking as rescale or fill-in scoaccess data could be busy along
    LOCK_XVALS();
    //every cleanup it is made sure sum == 1, so multiplication with sum is implicit
    float newxv = sco->getXVal() + num * discount_factor.value();
    sco->setXVal(newxv);
}

void
SCOCache::rescaleXVals_()
{
    // RWLock required (at least R)
    ASSERT_RWLOCKED();
    LOG_DEBUG("rescaling xvals");
    LOCK_XVALS();

    float xValSum = 0.0;
    for (NSMap::value_type& p: nsMap_)
    {
        for (SCOCacheNamespace::value_type& q: *(p.second))
        {
            xValSum += q.second.getSCO()->getXVal();
        }
    }

    VERIFY(xValSum >= 0.0);

    float min = std::numeric_limits<float>::max();
    int scoNum = scoCount_();

    for (NSMap::value_type& p: nsMap_)
    {
        SCOCacheNamespace* ns = p.second;
        for (SCOCacheNamespace::value_type& q: *ns)
        {
            SCOCacheNamespaceEntry& e = q.second;
            CachedSCOPtr sco = e.getSCO();
            float newVal = xValSum > 0 ?
                sco->getXVal() / xValSum :
                1.0 / scoNum;
            min = std::min<float>(min, newVal);
            sco->setXVal(newVal);
        }
    }

    VERIFY(min >= 0.0);
    cachedXValMin_ = scoNum ? min : 0;
    initialXVal_ = scoNum > 0 ? 1.0 / scoNum : 1.0;
}

float
SCOCache::getInitialXVal_()
{
    LOCK_XVALS();
    return initialXVal_;
}

void
SCOCache::fillSCOAccessData(SCOAccessData& sad)
{
    LOG_DEBUG("filling sco access data for " << sad.getNamespace());

    //always this order, xvallock to protect against rescaling while filling the data
    RLOCK_CACHE();
    LOCK_XVALS();

    SCOCacheNamespace* ns = findNamespace_throw_(sad.getNamespace());

    for (SCOCacheNamespace::value_type& t: *ns)
    {
        SCOCacheNamespaceEntry& e = t.second;
        CachedSCOPtr sco = e.getSCO();
        sad.addData(sco->getSCO(),
                    sco->getXVal());
    }
}

SCOCacheNamespace*
SCOCache::findNamespace_(const backend::Namespace& nsname)
{
    // RWLock required (R)
    ASSERT_RWLOCKED();

    NSMap::const_iterator it = nsMap_.find(nsname);
    if (it == nsMap_.end())
    {
        return nullptr;
    }

    return it->second;
}

SCOCacheNamespace*
SCOCache::findNamespace_throw_(const backend::Namespace& nsname)
{
    // RWLock required (R)
    ASSERT_RWLOCKED();

    SCOCacheNamespace* ns = findNamespace_(nsname);

    if (ns == 0)
    {
        LOG_ERROR("namespace " << nsname << " does not exist");
        throw fungi::IOException("namespace does not exist",
                                 nsname.c_str());
    }

    return ns;
}

bool
SCOCache::hasNamespace(const backend::Namespace& nsname)
{
    RLOCK_CACHE();
    return (findNamespace_(nsname) != nullptr);
}

bool
SCOCache::hasDisabledNamespace_(const backend::Namespace& nsname)
{
    ASSERT_NSPACE_MGMT_LOCKED();
    ASSERT_RWLOCKED();

    if (findNamespace_(nsname) == 0)
    {
        for (SCOCacheMountPointPtr mp: mountPoints_)
        {
            if (mp->hasNamespace(nsname))
            {
                return true;
            }
        }
    }

    return false;

}

bool
SCOCache::hasDisabledNamespace(const backend::Namespace& nsname)
{
    LOCK_NSPACE_MGMT();
    RLOCK_CACHE();
    return hasDisabledNamespace_(nsname);
}

void
SCOCache::setNamespaceLimits(const backend::Namespace& nsname,
                             uint64_t min,
                             uint64_t max)
{
    WLOCK_CACHE();

    SCOCacheNamespace* ns = findNamespace_throw_(nsname);
    ns->setLimits(min, max);
}

void
SCOCache::setNamespaceLimitMax(const backend::Namespace& nsname,
                               uint64_t max)
{
    WLOCK_CACHE();

    SCOCacheNamespace* ns = findNamespace_throw_(nsname);
    ns->setLimitMax(max);
}

void
SCOCache::addNamespace(const backend::Namespace& nsname,
                       uint64_t min,
                       uint64_t max_non_disposable)
{
    LOCK_NSPACE_MGMT();
    WLOCK_CACHE();

    if (findNamespace_(nsname) != 0)
    {
        LOG_ERROR("namespace " << nsname << " already exists");
        throw fungi::IOException("namespace already exists",
                                 nsname.c_str(),
                                 EEXIST);
    }

    if (hasDisabledNamespace_(nsname))
    {
        LOG_ERROR("namespace " << nsname << " already exists but is inactive");
        throw fungi::IOException("namespace already exists but is inactive",
                                 nsname.c_str(),
                                 EEXIST);
    }

    try
    {
        addNamespaceToMountPointsInList_(nsname, mountPoints_);
    }
    catch (...)
    {
        try
        {
            removeNamespaceFromMountPoints_(nsname);
        }
        catch (std::exception& e)
        {
            LOG_FATAL("failed to recover from failed namespace creation " <<
                      nsname << " - intervention required: " << e.what());
        throw;
        }
        catch (...)
        {
            LOG_FATAL("failed to recover from failed namespace creation " <<
                      nsname << " - intervention required");
            throw;
        }
        throw;
    }

    createAndInsertNamespace_(nsname, min, max_non_disposable);
}

void
SCOCache::removeDisabledNamespace(const backend::Namespace& nsname)
{
    LOCK_NSPACE_MGMT();

    SCOCacheMountPointList mplist;

    {
        RLOCK_CACHE();
        if (findNamespace_(nsname) != 0)
        {
            LOG_ERROR("namespace " << nsname <<
                      " is active, not removing it");
            throw fungi::IOException("namespace is active, not removing it",
                                     nsname.c_str());
        }

        mplist = mountPoints_;
    }

    removeNamespaceFromMountPointsInList_(nsname, mplist);
}

void
SCOCache::removeNamespace(const backend::Namespace& nsname)
{
    // Z42: LOCK_NSPACE_MGMT is probably not needed here, but will
    // not harm
    LOCK_NSPACE_MGMT();
    LOCK_CLEANUP();

    SCOCacheNamespace* ns = 0;

    SCOSet to_delete;
    {
        WLOCK_CACHE();

        ns = findNamespace_throw_(nsname);

        // Z42: add extra checks to avoid removal of namespaces with non-disposable
        // SCOs?
        for (SCOCacheNamespace::value_type& t: *ns)
        {
            SCOCacheNamespaceEntry& e = t.second;
            e.setBlocked(true);
            CachedSCO& sco = (*t.second.getSCO());
            to_delete.insert(sco);
        }
    }

    doCleanup_(to_delete, true);

    WLOCK_CACHE();

    // Neither prefetching nor normal volume ops are allowed during this call.
    // throw instead?
    VERIFY(ns->empty());

    delete ns;
    nsMap_.erase(nsname);

    try
    {
        removeNamespaceFromMountPoints_(nsname);
    }
    catch (std::exception& e)
    {
        LOG_FATAL("failed to remove namespace " << nsname <<
                  " from MountPoint(s) - intervention required: " << e.what());
        throw;
    }
    catch (...)
    {
        LOG_FATAL("failed to remove namespace " << nsname <<
                  " from MountPoint(s) - intervention required");
        throw;
    }
}

void
SCOCache::enableNamespace(const backend::Namespace& nsname,
                          uint64_t min,
                          uint64_t max_non_disposable,
                          const SCOAccessData& sad)
{
    LOCK_NSPACE_MGMT();
    WLOCK_CACHE();

    LOG_DEBUG(nsname << ": enabling");

    SCOCacheNamespace* ns = findNamespace_(nsname);
    if (ns != 0)
    {
        LOG_ERROR(nsname << ": already active");
        throw fungi::IOException("namespace already active",
                                 nsname.c_str(),
                                 EEXIST);
    }

    ns = createAndInsertNamespace_(nsname, min, max_non_disposable);

    try
    {
        for (SCOCacheMountPointPtr mp: mountPoints_)
        {
            if(mp->hasNamespace(nsname))
            {
                mp->scanNamespace(ns);
            }
            else
            {
                mp->addNamespace(nsname);
            }
        }
    }
    catch (std::exception& e)
    {
        LOG_ERROR(nsname << ": failed to activate namespace: " << e.what());
        disableNamespace_(ns);
        throw;
    }

    initXVals_(ns, sad);

    LOG_DEBUG(nsname << ": enabled");
}

void
SCOCache::disableNamespace(const backend::Namespace& nspace)
{
    LOCK_NSPACE_MGMT();
    WLOCK_CACHE();

    LOG_DEBUG(nspace << ": disabling");

    SCOCacheNamespace* ns = findNamespace_throw_(nspace);
    disableNamespace_(ns);

    LOG_DEBUG(nspace << ": disabled");
}

void
SCOCache::disableNamespace_(SCOCacheNamespace* ns)
{
    ASSERT_NSPACE_MGMT_LOCKED();
    ASSERT_RWLOCKED();

    nsMap_.erase(ns->getName());
    ns->clear();
    delete ns;
}

void
SCOCache::removeNamespaceFromMountPoints_(const backend::Namespace& nsname)
{
    ASSERT_RWLOCKED();
    removeNamespaceFromMountPointsInList_(nsname, mountPoints_);
}

void
SCOCache::removeNamespaceFromMountPointsInList_(const backend::Namespace& ns,
                                                const SCOCacheMountPointList& l)
{
    for (SCOCacheMountPointPtr mp: l)
    {
        if (mp->hasNamespace(ns))
        {
            mp->removeNamespace(ns);
        }
    }
}

void
SCOCache::addNamespaceToMountPointsInList_(const backend::Namespace& ns,
                                           const SCOCacheMountPointList& l)
{
    for (SCOCacheMountPointPtr mp: l)
    {
        if (not mp->hasNamespace(ns))
        {
            mp->addNamespace(ns);
        }
    }
}

SCOCacheNamespace*
SCOCache::createAndInsertNamespace_(const backend::Namespace& nsname,
                                    uint64_t min,
                                    uint64_t max_non_disposable)
{
    ASSERT_RWLOCKED();

    // extra paranoia
#ifndef NDEBUG
    NSMap::const_iterator it = nsMap_.find(nsname);
    VERIFY(it == nsMap_.end());
#endif

    SCOCacheNamespace* ns = new SCOCacheNamespace(nsname,
                                                  min,
                                                  max_non_disposable);
    std::pair<NSMap::iterator, bool> res =
        nsMap_.emplace(ns->getName(), ns);
    VERIFY(res.second);

    return ns;
}

void
SCOCache::getSCONameList(const backend::Namespace& nsname,
                         SCONameList& list,
                         bool disposable)
{
    RLOCK_CACHE();
    SCOCacheNamespace* ns = findNamespace_throw_(nsname);

    for (SCOCacheNamespace::value_type& t: *ns)
    {
        SCOCacheNamespaceEntry& e = t.second;
        CachedSCOPtr sco = e.getSCO();
        if (e.getSCO()->isDisposable() == disposable)
        {
            list.push_back(sco->getSCO());
        }
    }
}

void
SCOCache::getSCONameListAll(const backend::Namespace& nsname,
                            SCONameList& list)
{
    RLOCK_CACHE();
    SCOCacheNamespace* ns = findNamespace_throw_(nsname);

    for (SCOCacheNamespace::value_type& t: *ns)
    {
        SCOCacheNamespaceEntry& e = t.second;
        CachedSCOPtr sco = e.getSCO();
        list.push_back(sco->getSCO());
    }
}

CachedSCOPtr
SCOCache::createSCO_(const backend::Namespace& nsname,
                     SCO scoName,
                     uint64_t scoSize,
                     float xval,
                     bool blocked)
{
    ASSERT_RWLOCKED();

    SCOCacheNamespace* ns = findNamespace_throw_(nsname);
    if (ns->find(scoName) != ns->end())
    {
        const std::string s(nsname.str() + "/" + scoName.str());
        LOG_ERROR("attempt to create existing SCO " << s);
        throw fungi::IOException("attempt to create existing SCO",
                                 s.c_str(),
                                 EEXIST);
    }

    SCOCacheMountPointPtr mp = getWriteMountPoint_(scoSize);
    CachedSCOPtr sco = new CachedSCO(ns,
                                     scoName,
                                     mp,
                                     scoSize,
                                     xval);

    // we can do better by re-using the SCOCacheNamespace we looked up
    // above
    insertSCO_(sco, blocked);

    return sco;
}

CachedSCOPtr
SCOCache::createSCO(const backend::Namespace& nsname,
                    SCO scoName,
                    uint64_t scoSize)
{
    WLOCK_CACHE();

    CachedSCOPtr sco = createSCO_(nsname,
                                  scoName,
                                  scoSize,
                                  getInitialXVal_(),
                                  false);
    return sco;
}

CachedSCOPtr
SCOCache::findSCO_(const backend::Namespace& nsname,
                   SCO scoName)
{
    // RWLock required (at least R)
    ASSERT_RWLOCKED();

    SCOCacheNamespace* ns = findNamespace_throw_(nsname);
    SCOCacheNamespaceEntry *e = ns->findEntry(scoName);
    if (e)
    {
        if (e->isBlocked())
        {
            LOG_DEBUG("sco " << ns->getName() << "/" << scoName <<
                      " is currently being fetched");
            throw TransientException("sco is currently being fetched");
        }

        return e->getSCO();
    }
    else
    {
        return nullptr;
    }
}

CachedSCOPtr
SCOCache::findSCO(const backend::Namespace& nsname,
                  SCO scoName)
{
    RLOCK_CACHE();
    return findSCO_(nsname, scoName);
}

CachedSCOPtr
SCOCache::findSCO_throw(const backend::Namespace& nsname,
                        SCO scoName)
{
    auto sco(findSCO(nsname,
                     scoName));
    if (not sco)
    {
        LOG_ERROR(nsname << ": SCO " << scoName << " not found");
        throw SCONotFoundException("SCO not found");
    }

    return sco;
}

void
SCOCache::removeSCO(const backend::Namespace& nsname,
                    SCO scoName,
                    bool removeNonDisposable,
                    bool unlink )
{
    WLOCK_CACHE();

    findAndRemoveSCO_(nsname,
                      scoName,
                      removeNonDisposable,
                      false,
                      unlink);
}

CachedSCOPtr
SCOCache::getSCO_(const backend::Namespace& nsname,
                  SCO scoName,
                  uint64_t scoSize,
                  SCOFetcher& fetch,
                  float xval,
                  bool* cached,
                  bool /* prefetch */)
{
    if (cached)
    {
        *cached = false;
    }

    CachedSCOPtr sco;
    {
        // Z42: any way to RLOCK_CACHE() first and upgrade to WLOCK_CACHE() only if
        // we need to fetch it? boost::thread has the notion of upgradeable,
        // shared locks...
        WLOCK_CACHE();
        try
        {
            sco = findSCO_(nsname,
                           scoName);
            if (sco != nullptr)
            {
                LOG_DEBUG("findSCO_ " << nsname << ":" << scoName << " succeeded");
                if (cached)
                {
                    *cached = true;
                }

                return sco;
            }
        }
        catch (TransientException&)
        {
            LOG_DEBUG("findSCO_ " << nsname << ":" << scoName << " reported blocked SCO");
            throw;
        }

        LOG_DEBUG("findSCO_ " << nsname << ":" << scoName << " failed, fetching it");
        sco = createSCO_(nsname,
                         scoName,
                         scoSize,
                         xval,
                         true);
    }

    try
    {
        fetch(sco->path());
        LOG_DEBUG("fetching SCO " << nsname << ":" <<
                  scoName << " succeeded");

    }
    catch (SCOCacheMountPointIOException& e)
    {
        reportIOError(sco);
        throw TransientException("Retryable I/O error");
    }
    catch(std::exception& e)
    {
        LOG_ERROR("fetching SCO " << nsname << ":" <<
                  scoName <<
                  " failed: " << e.what());

        WLOCK_CACHE();
        removeSCO_(sco, false);
        throw;

    }

    catch (...)
    {
        LOG_ERROR("fetching SCO " << nsname << ":" <<
                  scoName <<
                  " failed, unkown exception");
        WLOCK_CACHE();
        removeSCO_(sco, false);
        throw;
    }

    WLOCK_CACHE();
    if (fetch.disposable())
    {
        try
        {
            sco->setDisposable();
        }
        catch (std::exception& e)
        {
            reportIOError_(sco);
            throw;
        }
    }

    // while the SCO is fetched and the rwlock was released, the mountpoint
    // could've gone bad, so we need to do another lookup
    try
    {
        SCOCacheNamespaceEntry* e = sco->getNamespace()->findEntry_throw(sco->getSCO());
        e->setBlocked(false);
    }
    CATCH_STD_ALL_LOG_RETHROW("Problem with mountpoint when getting SCO " <<
                              scoName << " From the FOC");

    return sco;
}

CachedSCOPtr
SCOCache::getSCO(const backend::Namespace& nsname,
                 SCO scoName,
                 uint64_t scoSize,
                 SCOFetcher& fetch,
                 bool* cached)
{
    return getSCO_(nsname,
                   scoName,
                   scoSize,
                   fetch,
                   getInitialXVal_(),
                   cached,
                   false);
}

bool
SCOCache::softCacheFull_() const
{
    //Determines whether the SCOcache is full considering the soft trigger and backoffgap bounds.
    //The cache is considered full if it in in regime which is indicated by free space < backoffSize on all mountpoints.

    RLOCK_CACHE();
    for (SCOCacheMountPointPtr mp: mountPoints_)
    {
        if (mp->getFreeDiskSpace() > backoff_gap.value().getBytes())
        {
            return false;
        }
    }
    return true;
}


bool
SCOCache::prefetchSCO(const backend::Namespace& nsname,
                      SCO scoName,
                      uint64_t scoSize,
                      float sap,
                      SCOFetcher& fetch)
{
    if (softCacheFull_() && sap < cachedXValMin_)
    {
        LOG_DEBUG(nsname << "/" << scoName << ": not prefetching, sap " <<
                  sap << " < " << cachedXValMin_);
        return false;
    }

    LOG_DEBUG(nsname << "/" << scoName << ": trying to prefetech, sap " << sap);

    try
    {
        CachedSCOPtr sco = getSCO_(nsname,
                                   scoName,
                                   scoSize,
                                   fetch,
                                   sap,
                                   0,
                                   true);
        LOG_DEBUG(nsname << "/" << scoName << ": successfully prefetched");
    }
    catch (TransientException& e)
    {
        // either the sco is being fetched concurrently, or the cache has filled
        // up -- do we want to stop prefetching in the latter case (return false)?
        LOG_DEBUG("Caught transient exception while prefetching " << nsname <<
                  "/" << scoName << ": " << e.what() <<
                  " - Continuing");
        return true;

    }
    catch (std::exception& e)
    {
        // Gah. Might have been an i/o error on a mountpoint? If so it will have
        // been offlined, so let's continue.
        LOG_WARN("Caught exception while prefetching " << nsname << "/" <<
                 scoName << ": " << e.what() <<
                 " - Continuing");
        return true;
    }
    catch (...)
    {
        LOG_WARN("Caught unknown exception while prefetching " << nsname <<
                 "/" << scoName);
        return false;
    }

    return true;
}

void
SCOCache::setSCODisposable(CachedSCOPtr sco)
{
    RLOCK_CACHE();
    sco->setDisposable();
}

bool
SCOCache::isSCODisposable(CachedSCOPtr sco)
{
    RLOCK_CACHE();
    return sco->isDisposable();
}

void
SCOCache::reportIOError(CachedSCOPtr sco)
{
    WLOCK_CACHE();
    reportIOError_(sco);
}

void
SCOCache::reportIOError_(CachedSCOPtr sco)
{
    ASSERT_RWLOCKED();

    SCOCacheMountPointPtr mp = sco->getMountPoint();
    LOG_ERROR("I/O error in SCO " << sco->path());
    offlineMountPoint_(mp);
}

namespace
{
struct MountPointEqualsPred
{
    MountPointEqualsPred(SCOCacheMountPointPtr mp)
        : mp_(mp)
    {}

    bool
    operator()(SCOCacheMountPointPtr mp)
    {
        return (mp == mp_);
    }

    SCOCacheMountPointPtr mp_;
};
}

void
SCOCache::bumpMountPointErrorCount_()
{
    ASSERT_RWLOCKED();

    ++mpErrorCount_;

    SCOCacheMountPointList::iterator it = mountPoints_.begin();
    while (it != mountPoints_.end())
    {
        SCOCacheMountPointPtr mp = *it;
        try
        {
            mp->setErrorCount(mpErrorCount_);
            ++it;
        }
        catch (std::exception& e)
        {
            LOG_ERROR("Failed to set error count of mountpoint " << mp->getPath() <<
                      " to " << mpErrorCount_ << ": " << e.what());
            offlineMountPoint_(mp);
            it = mountPoints_.begin();
            currentMountPoint_ = mountPoints_.begin();
        }
    }
}

void
SCOCache::offlineMountPoint_(SCOCacheMountPointPtr mp)
{
    ASSERT_RWLOCKED();

    LOG_INFO("Offlining mountpoint " << mp->getPath());

    mp->setOffline();

    size_t old_mp_cnt = mountPoints_.size();

    MountPointEqualsPred pred(mp);
    mountPoints_.remove_if(pred);

    if (mountPoints_.size() == old_mp_cnt)
    {
        LOG_DEBUG(mp->getPath() <<
                  ": MountPoint already removed, nothing left to do");
        return;
    }

    VolumeDriverError::report(events::VolumeDriverErrorCode::SCOCacheMountPointOfflined,
                              mp->getPath().string());

    // don't try to unlink SCOs from the filesystem -- the mountpoint was
    // very likely remounted readonly so we'd get a barrage of exceptions.
    // just remove the references from the NSMap.
    for (NSMap::value_type& t: nsMap_)
    {
        SCOCacheNamespace* ns = t.second;
        SCOCacheNamespace::iterator it = ns->begin();
        while (it != ns->end())
        {
            // Z42: rethink the removal of "blocked" SCOs (those under I/O)
            if (it->second.getSCO()->getMountPoint() == mp)
            {
                SCOCacheNamespace::iterator tmp = it;
                ++it;
                ns->erase(tmp);
            }
            else
            {
                ++it;
            }
        }
    }

    currentMountPoint_ = mountPoints_.begin();
    bumpMountPointErrorCount_();
}

void
SCOCache::maybeChokeNamespaces_()
{
    ASSERT_RWLOCKED();

    for (NSMap::value_type& t: nsMap_)
    {
        SCOCacheNamespace* ns = t.second;
        uint64_t nondisposable = 0;

        // doesn't reuse getNamespaceInfo as that looks at the real SCO size
        for (SCOCacheNamespace::value_type& u: *ns)
        {
            SCOCacheNamespaceEntry& e = u.second;
            CachedSCOPtr sco = e.getSCO();
            if (not sco->isDisposable())
            {
                nondisposable += sco->getSize();
            }
        }

        bool choke = nondisposable > ns->getMaxNonDisposableSize();

        if (choke != ns->isChoking())
        {
            LOG_INFO("Namespace " << ns->getName() << " non-disposable: " <<
                     (nondisposable >> 20) << " MiB, max: " <<
                     (ns->getMaxNonDisposableSize() >> 20) << " - " <<
                     (choke ? "" : "un") << "choking it");
        }
        else
        {
            LOG_DEBUG("Namespace " << ns->getName() << " non-disposable: " <<
                      (nondisposable >> 20) << " MiB, max: " <<
                      (ns->getMaxNonDisposableSize() >> 20) << " - " <<
                      (choke ? "" : "not ") << " choking");
        }

        ns->setChoking(choke);
    }
}

// has the side effect of un-choking mountpoints
bool
SCOCache::checkForWork_()
{
    ASSERT_RWLOCKED();

    bool ret = false;

    // operate on a snapshot of mountPoints_ that cannot be changed behind our back
    SCOCacheMountPointList mps = mountPoints_;

    for (SCOCacheMountPointPtr mp: mps)
    {
        if (not mp->isOffline())
        {
            uint64_t capacity = mp->getCapacity();
            // look at the SCOCacheNamespaces instead?
            uint64_t used = mp->getUsedSize();
            uint64_t freespace;
            uint64_t gap;

            try
            {
                freespace = mp->getFreeDiskSpace();
            }
            catch (std::exception& e)
            {
                // huh? we should try to offline the mountpoint
                LOG_ERROR(mp->getPath() << ": failed to determine free space: " <<
                          e.what());
                offlineMountPoint_(mp);
                continue;
            }

            gap = std::min<uint64_t>(freespace,
                                     capacity - used);

            LOG_PERIODIC(mp->getPath() << ": max size " << (capacity >> 20) <<
                         "MiB, used " << (used >> 20) << "MiB, free_on_disk " <<
                         (freespace >> 20) << "MiB, gap " << (gap >> 20) << "MiB");

            if (gap < trigger_gap.value().getBytes())
            {
                LOG_PERIODIC(mp->getPath() << ": cleanup required");
                ret = true;
            }
            else
            {
                mp->setNoChoking();
            }
        }
    }

    return ret;
}

void
SCOCache::cleanup()
{
    LOCK_CLEANUP();

    SCOSet to_delete;
    {
        WLOCK_CACHE();

        LOG_PERIODIC("starting");

        maybeChokeNamespaces_();

        if (checkForWork_())
        {
            try
            {
                prepareCleanup_(to_delete);
            }
            catch (std::exception& e)
            {
                LOG_ERROR("cleanup failed: " << e.what());
            }
            catch (...)
            {
                LOG_ERROR("cleanup failed: unknown exception");
            }
        }
        else
        {
            LOG_PERIODIC("no cleanup required");
        }

        rescaleXVals_();
    }

    LOG_PERIODIC("done");

    doCleanup_(to_delete, false);

    RLOCK_CACHE();
    // abused to print usage info - *brr*
    checkForWork_();
}

void
SCOCache::doCleanup_(SCOSet& to_delete,
                     bool remove_non_disposable)
{
    ASSERT_CLEANUP_LOCKED();

    SCOSet::iterator it = to_delete.begin();

    while (it != to_delete.end())
    {
        WLOCK_CACHE();
        eraseSCOSetIterator_(to_delete, it, remove_non_disposable);
        it = to_delete.begin();
    }
}

void
SCOCache::prepareCleanup_(SCOSet& to_delete)
{
    ASSERT_CLEANUP_LOCKED();
    ASSERT_RWLOCKED();

    // trim namespaces:
    NSSCOSets nsSCOSets;

    for (NSMap::value_type& p: nsMap_)
    {
        SCOCacheNamespace* ns = p.second;

        SCOSet* scoSet = new SCOSet();

        std::pair<NSSCOSets::iterator, bool> res =
            nsSCOSets.insert(std::make_pair(ns, scoSet));
        VERIFY(res.second);

        uint64_t totalSize = 0;
        uint64_t disposableSize = 0;
        for (SCOCacheNamespace::value_type& q: *ns)
        {
            SCOCacheNamespaceEntry& e = q.second;
            CachedSCOPtr sco = e.getSCO();

            // usecount: sco, entry in SCOCacheNamespace in nsMap_
            VERIFY(sco->use_count() >= 2);

            // e.isBlocked is actually currently redundant - the use count
            // should not allow these to enter. But lets explicitly test it just in
            // case that changes at some point
            if (sco->isDisposable() && sco->use_count() == 2 && !e.isBlocked())
            {
                scoSet->insert(*sco);
                disposableSize += sco->getSize();
            }

            totalSize += sco->getSize();
        }

        ensureNamespaceMin_(ns, *scoSet, totalSize, disposableSize);
    }

    // don't checkForWork_() but rather go all the way through
    trimMountPoints_(nsSCOSets, to_delete);

    for (NSSCOSets::value_type& p: nsSCOSets)
    {
        SCOSet* scoSet = p.second;
        delete scoSet;
    }
}

uint64_t
SCOCache::eraseSCOSetIterator_(SCOSet& scoSet,
                               SCOSet::iterator& it,
                               bool remove_non_disposable)
{
    ASSERT_CLEANUP_LOCKED();
    ASSERT_RWLOCKED();

    SCOCacheMountPointPtr mp = it->getMountPoint();
    CachedSCO& sco = *it;
    VERIFY(sco.use_count() == 1);

    uint64_t scoSize = sco.getSize();
    fs::path scoPath(sco.path());

    scoSet.erase(it);

    // (1) don't use "sco" after findAndRemoveSCO_ anymore
    // (2) exceptions: there really shouldn't be any
    //     - if a previous I/O error removed the SCO we should not be here in the
    //       first place since it'll be auto-unlinked from the intrusive set
    //     - failure to delete the SCO from the filesystem is swallowed in the
    //       destructor - hence we need to check below.
    //     - SCOs in state blocked have no business in the scoSet - this needs to
    //       be guaranteed by callers
    findAndRemoveSCO_(sco.getNamespace()->getName(),
                      sco.getSCO(),
                      remove_non_disposable,
                      true);
    if (fs::exists(scoPath))
    {
        LOG_ERROR("failed to remove SCO " << scoPath);
        offlineMountPoint_(mp);
        return 0;
    }

    return scoSize;
}

void
SCOCache::ensureNamespaceMin_(SCOCacheNamespace* ns,
                              SCOSet& scoSet,
                              uint64_t totalSize,
                              uint64_t disposableSize)
{
    ASSERT_CLEANUP_LOCKED();
    ASSERT_RWLOCKED();

    // enforce min sizes by preserving the disposable scos with the highest
    // access probabilities
    uint64_t min = ns->getMinSize();
    uint64_t nonDisposableSize = totalSize - disposableSize;

    int64_t preserve = (nonDisposableSize >= min) ?
        0 :
        min - nonDisposableSize;

    SCOSet::reverse_iterator rit = scoSet.rbegin();

    while (rit != scoSet.rend() && preserve > 0)
    {
        CachedSCO& sco = *rit;
        preserve -= sco.getSize();
        // gah - scoSet.erase(rit) does not work
        //scoSet.erase(rit);
        sco.bi::set_base_hook<bi::link_mode<bi::auto_unlink> >::unlink();
        rit = scoSet.rbegin();
    }
}

void
SCOCache::trimMountPoints_(NSSCOSets& nsSCOSets, SCOSet& to_delete)
{
    ASSERT_CLEANUP_LOCKED();
    ASSERT_RWLOCKED();

    // heck, we could even check the mountpoints idea of used size against
    // the view of the nsMap_ ...
    // will keeping a permanent intrusive list at the mountpoint (which needs
    // to be sorted on cleanup) give any benefits over the below approach?
    typedef std::map<const SCOCacheMountPointPtr, SCOSet*> MPSCOSets;
    MPSCOSets mpSCOSets;

    for (SCOCacheMountPointPtr mp: mountPoints_)
    {
        std::pair<MPSCOSets::iterator, bool> res =
            mpSCOSets.insert(std::make_pair(mp, new SCOSet()));
        VERIFY(res.second);
    }

    for (NSSCOSets::value_type& p: nsSCOSets)
    {
        SCOSet* nsSCOSet = p.second;

        SCOSet::iterator nsit = nsSCOSet->begin();
        while (nsit != nsSCOSet->end())
        {
            CachedSCO& sco = *nsit;

            // namespace trimming happened before and must have ensured that
            VERIFY(sco.isDisposable());
            VERIFY(sco.use_count() == 1);

            MPSCOSets::iterator mpit = mpSCOSets.find(sco.getMountPoint());
            VERIFY(mpit != mpSCOSets.end());

            SCOSet* mpSCOSet = mpit->second;
            nsSCOSet->erase(nsit);
            nsit = nsSCOSet->begin();

            mpSCOSet->insert(sco);

        }
    }

    MPSCOSets::iterator it = mpSCOSets.begin();
    while (it != mpSCOSets.end())
    {
        SCOSet* scoSet = it->second;

        trimMountPoint_(it->first, *scoSet, to_delete);

        mpSCOSets.erase(it);
        delete scoSet;

        it = mpSCOSets.begin();
    }
}

void
SCOCache::trimMountPoint_(const SCOCacheMountPointPtr mp,
                          SCOSet& mpSCOSet,
                          SCOSet& to_delete)
{
    ASSERT_CLEANUP_LOCKED();
    ASSERT_RWLOCKED();

    LOG_DEBUG("trimming " << mp->getPath());

    // gah - yet another lookup of this
    uint64_t freespace = mp->getFreeDiskSpace();
    freespace = std::min<uint64_t>(freespace,
                                   mp->getCapacity() - mp->getUsedSize());

    SCOSet::iterator it = mpSCOSet.begin();

    if (freespace < trigger_gap.value().getBytes())
    {
        while (it != mpSCOSet.end() && freespace < backoff_gap.value().getBytes())
        {
            CachedSCO& sco = *it;
            VERIFY(sco.use_count() == 1);
            VERIFY(sco.isDisposable());

            SCOCacheNamespaceEntry* e =
                sco.getNamespace()->findEntry(sco.getSCO());
            VERIFY(e != nullptr);
            e->setBlocked(true);

            mpSCOSet.erase(it);
            it = mpSCOSet.begin();

            to_delete.insert(sco);

            freespace += sco.getSize();
        }

        if (freespace < trigger_gap.value().getBytes())
        {
            //set choking of mountpoint dependent on the amount of space left
            uint32_t adaptedThrottling;
            double maxThrot = 1000000;

            if(freespace != 0) {
                float factor = float(trigger_gap.value().getBytes()) / float(freespace);
                adaptedThrottling = uint32_t(std::min(maxThrot,
                                                      double(datastore_throttle_usecs.value().load()) * factor));
            }
            else {
                adaptedThrottling = uint32_t(maxThrot);
            }
            mp->setChoking(adaptedThrottling);

            LOG_INFO(mp->getPath() << " is choking: free " << (freespace >> 20) <<
                     "MiB < trigger " << (trigger_gap.value().getBytes() >> 20) << "MiB." << " Throttling ingest with " << adaptedThrottling << " usec per cluster write.");
        }
    }
}

SCOCacheNamespaceInfo
SCOCache::getNamespaceInfo(const Namespace& name)
{
    RLOCK_CACHE();

    SCOCacheNamespace* ns = findNamespace_throw_(name);

    SCOCacheNamespaceInfo nsi(ns->getName(),
                              ns->getMinSize(),
                              ns->getMaxNonDisposableSize());

    for (SCOCacheNamespace::value_type& q: *ns)
    {
        CachedSCOPtr sco = q.second.getSCO();

        if (sco->isDisposable())
        {
            nsi.disposable += sco->getRealSize();
        }
        else
        {
            nsi.nondisposable += sco->getRealSize();
        }
    }
    return nsi;
}

void
SCOCache::getMountPointsInfo(SCOCacheMountPointsInfo& info)
{
    RLOCK_CACHE();

    for (SCOCacheMountPointPtr &mp: mountPoints_)
    {
        SCOCacheMountPointInfo mpinfo(mp->getPath(),
                                      mp->getCapacity(),
                                      mp->getFreeDiskSpace(),
                                      mp->getUsedSize(),
                                      mp->isChoking());
        info.insert(std::make_pair(mpinfo.path, mpinfo));
    }

    for (auto mp: scocache_mount_points.value())
    {
        SCOCacheMountPointInfo mpinfo(mp.path, true);
        if (info.find(mpinfo.path) == info.end())
        {
            info.insert(std::make_pair(mpinfo.path, mpinfo));
        }
    }
    //TODO [bdv] verify that mountPoints_ is a subset of scocache_mount_points
}

// Z42: make const
void
SCOCache::dumpSCOInfo(const fs::path& outfile)
{
    RLOCK_CACHE();

    std::ofstream f(outfile.string().c_str());

    f << "NSpace,SCOName,SCOSize,disposable,MntPoint,Sap:" << std::endl;

    for (NSMap::value_type& p: nsMap_)
    {
        SCOCacheNamespace* ns = p.second;

        for (SCOCacheNamespace::value_type& q: *ns)
        {
            SCOCacheNamespaceEntry& e = q.second;
            CachedSCOPtr sco = e.getSCO();

            f << sco->getNamespace()->getName() << "," <<
                sco->getSCO() << "," <<
                sco->getSize() << "," <<
                sco->isDisposable() << "," <<
                sco->getMountPoint()->getPath() << "," <<
                sco->getXVal() << std::endl;
        }
    }

    f.close();
}

bool
SCOCache::hasMountPoint(const fs::path& p) const {
    //TODO [bdv] use find or one of its friends?
    for (SCOCacheMountPointPtr mp: mountPoints_)
    {
        if (mp->getPath() == p)
        {
            return true;
        }
    }
    return false;
}


void
SCOCache::addMountPoint(const MountPointConfig& cfg)
{
    TODO("AR: Fix adding mountpoints!");
    using T = decltype(scocache_mount_points)::ValueType;
    const_cast<T&>(scocache_mount_points.value()).push_back(cfg);
    _addMountPoint(cfg);

    // // auxiliary function to keep the tests going, might disappear later
    // scocache_mount_points.value().push_back(cfg);
    // _addMountPoint(cfg);
}

void
SCOCache::_addMountPoint(const MountPointConfig& cfg)
{
    WLOCK_CACHE();
    if (hasMountPoint(cfg.path))
    {
        LOG_ERROR(cfg.path << " already exists");
        throw fungi::IOException("mountpoint already exists",
                                 cfg.path.string().c_str(),
                                 EEXIST);
    }

    SCOCacheMountPointPtr mp = new SCOCacheMountPoint(*this,
                                                      cfg,
                                                      false);
    mp->newMountPointStage2(mpErrorCount_);
    mountPoints_.push_back(mp);

    for (NSMap::value_type& p: nsMap_)
    {
        try
        {
            mp->addNamespace(p.first);
        }
        catch (std::exception& e)
        {
            LOG_ERROR(mp->getPath() << ": failed to add namespace " << p.first <<
                      ": " << e.what());
            removeMountPoint_(cfg.path, false);
            throw;
        }
    }

    if (mountPoints_.size() == 1)
    {
        // all mountpoints were gone - reinitialize currentMountPoint_
        // now that we're back in business.
        currentMountPoint_ = mountPoints_.begin();
    }
}

void
SCOCache::removeMountPoint(const fs::path& path)
{
    WLOCK_CACHE();
    removeMountPoint_(path, false);
}

void
SCOCache::removeMountPoint_(const fs::path& path, bool force)
{
    ASSERT_RWLOCKED();

    for (SCOCacheMountPointList::iterator it = mountPoints_.begin();
         it != mountPoints_.end();
         ++it)
    {
        SCOCacheMountPointPtr mp = *it;

        if (mp->getPath() == path)
        {
            if (!force && mountPoints_.size() == 1)
            {
                LOG_ERROR("cannot remove last mountpoint " << path);
                throw fungi::IOException("cannot remove last mountpoint",
                                         path.string().c_str(),
                                         EBUSY);
            }

            for (NSMap::value_type& p: nsMap_)
            {
                SCOCacheNamespace* ns = p.second;

                std::list<CachedSCOPtr> l;

                for (SCOCacheNamespace::value_type& q: *ns)
                {
                    SCOCacheNamespaceEntry& e = q.second;
                    CachedSCOPtr sco = e.getSCO();

                    if (!force && sco->getMountPoint() == mp &&
                        !sco->isDisposable())
                    {
                        LOG_ERROR("cannot remove " << path <<
                                  ": not all SCOs in backend yet");
                        throw fungi::IOException("cannot remove mountpoint",
                                                 path.string().c_str(),
                                                 EBUSY);
                    }

                    // use count: SCOCacheNamespace, sco -> 2
                    if (sco->use_count() > 2)
                    {
                        LOG_ERROR("cannot remove " << path <<
                                  ": SCO(s) still in use");

                        throw fungi::IOException("cannot remove mountpoint",
                                                 path.string().c_str(),
                                                 EBUSY);
                    }

                    l.push_back(sco);
                }

                // we can do better by getting iterators and erasing these
                for (CachedSCOPtr sco: l)
                {
                    removeSCO_(sco, false);
                }
            }

            // don't forget to update currentMountPoint_ if necessary:
            if (currentMountPoint_ == it)
            {
                ++currentMountPoint_;

                if (currentMountPoint_ == mountPoints_.end())
                {
                    currentMountPoint_ = mountPoints_.begin();
                }
            }

            mountPoints_.erase(it);
            return;
        }
    }

    LOG_ERROR("mountpoint " << path << " does not exist");
    throw fungi::IOException("mountpoint does not exist",
                             path.string().c_str(),
                             ENOENT);
}

int
SCOCache::scoCount_() const
{
    int scos = 0;
    for (const NSMap::value_type& p: nsMap_)
    {
        SCOCacheNamespace* ns = p.second;
        scos += ns->size();
    }
    return scos;
}

void
SCOCache::update(const boost::property_tree::ptree& pt,
                 UpdateReport& rep)
{
    backoff_gap.update(pt, rep);
    trigger_gap.update(pt, rep);
    discount_factor.update(pt, rep);

    //TODO reporting + different report for new mountpoints versus existing ones
    scocache_mount_points.update(pt, rep);

    for (auto mp: scocache_mount_points.value())
    {
        if (not hasMountPoint(mp.path)) {
            try {
                _addMountPoint(mp);
            } catch (...) {
                LOG_WARN("Failed to add mountpoint " << mp.path);
            }
        }
    }
}

void
SCOCache::persist(boost::property_tree::ptree& pt,
                  const ReportDefault reportDefault) const
{
    backoff_gap.persist(pt,
                        reportDefault);
    trigger_gap.persist(pt,
                        reportDefault);
    discount_factor.persist(pt,
                            reportDefault);
    scocache_mount_points.persist(pt,
                                  reportDefault);
}

bool
SCOCache::checkConfig(const boost::property_tree::ptree& pt,
                      ConfigurationReport& rep) const
{
    bool result = checkParameterConfig(pt, rep);
    /// We need different thing here for finding... should be on key...
    PARAMETER_TYPE(scocache_mount_points) scocache_mount_points_local(pt);
    for (auto mp: scocache_mount_points.value())
    {
        if(std::find(scocache_mount_points_local.value().begin(),
                scocache_mount_points_local.value().begin(),
                mp) == scocache_mount_points_local.value().end())
        {
            result = false;
            std::stringstream ss;
            ss << "Removal of mountpoint " << mp.path << " not supported.";
            ConfigurationProblem problem(PARAMETER_TYPE(scocache_mount_points)::name(),
                                         componentName(),
                                         ss.str());
            LOG_WARN(ss.str());
        }
    }
    return result;
}

bool
SCOCache::checkParameterConfig(const boost::property_tree::ptree& pt,
                               ConfigurationReport& rep) const
{
    bool result = true;

    PARAMETER_TYPE(backoff_gap) backoff_gap_local(pt);
    PARAMETER_TYPE(trigger_gap) trigger_gap_local(pt);
    if(backoff_gap_local.value().getBytes() == 0)
    {
        rep.emplace_back(PARAMETER_TYPE(backoff_gap)::name(),
                         componentName(),
                         "backoff gap is 0");

        result = false;

    }
    if(trigger_gap_local.value().getBytes() == 0)
    {
        rep.emplace_back(PARAMETER_TYPE(trigger_gap)::name(),
                         componentName(),
                         "trigger gap is 0");

        result = false;

    }

    if(backoff_gap_local.value().getBytes() < trigger_gap_local.value().getBytes())
    {
        rep.emplace_back(PARAMETER_TYPE(trigger_gap)::name() +
                         std::string(", ") +
                         PARAMETER_TYPE(trigger_gap)::name(),
                         componentName(),
                         "backoff gap is strictly smaller than trigger gap");

        result = false;
    }

    return result;
}

}

// Local Variables: **
// mode: c++ **
// End: **
