// Copyright 2015 iNuron NV
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

#ifndef SCO_CACHE_H_
#define SCO_CACHE_H_

#include "CachedSCO.h"
#include "SCOCacheMountPoint.h"
#include "SCOCacheNamespace.h"
#include "SCOAccessData.h"
#include "SCOFetcher.h"
#include "SCOCacheInfo.h"
#include "Types.h"
#include "VolumeDriverParameters.h"

#include <list>
#include <vector>

#include <boost/intrusive/set.hpp>
#include <boost/thread/mutex.hpp>

#include <youtils/RWLock.h>
#include <youtils/SpinLock.h>
#include <youtils/VolumeDriverComponent.h>

#include <backend/BackendInterface.h>

namespace volumedriver
{

class SCOCacheTestSetup;

class SCOCache
    : public VolumeDriverComponent
{
    friend class SCOCacheTestSetup;
    friend class SCOCacheTest;
    friend class ErrorHandlingTest;
    friend class VolManagerTestSetup;
    friend class VolManager;

public:
    MAKE_EXCEPTION(SCONotFoundException,
                   fungi::IOException);

    typedef std::list<SCOCacheMountPointPtr> SCOCacheMountPointList;

    explicit SCOCache(const boost::property_tree::ptree& pt);

    SCOCache(const SCOCache&) = delete;
    SCOCache& operator=(const SCOCache&) = delete;

    ~SCOCache();

    // auxiliary function to keep the tests going, might disappear later
    void
    addMountPoint(const MountPointConfig& cfg);

    // End VolumeDriverComponent Interface
    CachedSCOPtr
    createSCO(const backend::Namespace& nspace,
              SCO scoName,
              uint64_t scoSize);

    CachedSCOPtr
    getSCO(const backend::Namespace& nspace,
           SCO scoName,
           uint64_t scoSize,
           SCOFetcher& fetch,
           bool* cached = 0);

    CachedSCOPtr
    findSCO(const backend::Namespace& nspace,
            SCO sco);

    CachedSCOPtr
    findSCO_throw(const backend::Namespace& nspace,
                  SCO sco);

    bool
    prefetchSCO(const backend::Namespace& nspace,
                SCO scoName,
                uint64_t scoSize,
                float sap,
                SCOFetcher& fetch);

    // consumers of SCOCache need to use these 2 to be protected against races,
    // see CachedSCO.h
    void
    setSCODisposable(CachedSCOPtr sco);

    bool
    isSCODisposable(CachedSCOPtr sco);

    void
    removeSCO(const backend::Namespace&,
              SCO scoName,
              bool removeNonDisposable,
              bool unlink = true);

    void
    getSCONameList(const backend::Namespace&,
                   SCONameList& list,
                   bool disposable);

    void
    getSCONameListAll(const backend::Namespace&,
                      SCONameList& list);

    void
    addNamespace(const backend::Namespace&,
                 uint64_t min,
                 uint64_t max_non_disposable);

    void
    removeNamespace(const backend::Namespace&);

    void
    removeDisabledNamespace(const backend::Namespace&);

    void
    enableNamespace(const backend::Namespace&,
                    uint64_t min,
                    uint64_t max_non_disposable,
                    const SCOAccessData& sad);

    void
    disableNamespace(const backend::Namespace& nspace);

    // Z42: merge these 2 into one method returning a boost::tribool?
    bool
    hasNamespace(const backend::Namespace&);

    bool
    hasDisabledNamespace(const backend::Namespace&);

    void
    setNamespaceLimits(const backend::Namespace&,
                       uint64_t min,
                       uint64_t max);

    void
    setNamespaceLimitMax(const backend::Namespace&,
                         uint64_t max);

    void
    reportIOError(CachedSCOPtr);

    void
    removeMountPoint(const boost::filesystem::path& mountPointPath);

    void
    cleanup();

    void
    fillSCOAccessData(SCOAccessData& sad);

    void
    signalSCOAccessed(CachedSCOPtr sco, uint32_t num);

    void
    insertScannedSCO(CachedSCOPtr sco);

    SCOCacheNamespaceInfo
    getNamespaceInfo(const backend::Namespace& ns);

    void
    getMountPointsInfo(SCOCacheMountPointsInfo& info);

    void
    dumpSCOInfo(const boost::filesystem::path& outfile);

    uint32_t
    getThrottleUsecs() const
    {
        return datastore_throttle_usecs.value();
    }

    bool
    hasMountPoint(const boost::filesystem::path& p) const;

private:
    DECLARE_LOGGER("SCOCache");

    void
    _addMountPoint(const MountPointConfig& cfg);


    // VolumeDriverComponent Interface
    virtual const char*
    componentName() const override final
    {
        return initialized_params::scocache_component_name;
    }


    virtual void
    update(const boost::property_tree::ptree&,
           UpdateReport& report) override final;


    virtual void
    persist(boost::property_tree::ptree&,
            const ReportDefault reportDefault = ReportDefault::F) const override final;

    virtual bool
    checkConfig(const boost::property_tree::ptree&,
                ConfigurationReport&) const override final;

    virtual bool
    checkParameterConfig(const boost::property_tree::ptree&,
                         ConfigurationReport&) const final;


    // LOCKING NOTES:
    // - rwLock_ protects SCOs in namespaces, mountpoints. Needs to be
    //   write-locked if any of these need to be modified (addition /
    //   removal of items, change of currentMountPoint_), otherwise
    //   read-locked.
    // - xValSpinLock_ protects xvals (SumMinMax)
    // - cleanupLock_ prevents namespace removal vs. cache cleanup races
    //   (both generate an intrusive set).
    // - nspaceMgmtLock_ serializes management calls on inactive
    //   namespaces (migration of SCOs vs. enabling / disabling
    //   namespaces). This one is there to avoid interfering with the
    //   data path (protected by rwLock_), while migrating SCOs between
    //   namespaces which is potentially costly.
    //   Other, cheaper mgmt calls on _active_ namespaces (except
    //   disabling them, for apparent reasons) are protected by the
    //   rwLock_
    //
    // ORDER:
    // (1) nspaceMgmtLock_
    // (2) cleanupLock_
    // (3) rwLock_
    // (4) xValSpinLock_
    //
    // Z42: merge nspaceMgmtLock_ and cleanupLock_ ?

    typedef boost::mutex cleanup_lock_type;
    typedef boost::mutex nspace_mgmt_lock_type;

    mutable cleanup_lock_type cleanupLock_;
    mutable nspace_mgmt_lock_type nspaceMgmtLock_;
    mutable fungi::RWLock rwLock_;
    mutable fungi::SpinLock xValSpinLock_;

    SCOCacheMountPointList mountPoints_;
    SCOCacheMountPointList::const_iterator currentMountPoint_;

    typedef std::map<const Namespace, SCOCacheNamespace*> NSMap;
    NSMap nsMap_;


    float cachedXValMin_;
    float initialXVal_;


    uint64_t mpErrorCount_;

    // void
    // validateParams_();

    void
    destroy_();

    bool
    checkForWork_();

    struct SCOCompare
    {
        bool operator()(const CachedSCO& lhs, const CachedSCO& rhs) const
        {
            return lhs.getXVal() < rhs.getXVal();
        }
    };

    typedef boost::intrusive::multiset<CachedSCO,
                         boost::intrusive::compare<SCOCompare>,
                         boost::intrusive::constant_time_size<false> > SCOSet;

    void
    prepareCleanup_(SCOSet& to_delete);

    void
    doCleanup_(SCOSet& to_delete,
               bool remove_non_disposable);

    SCOCacheNamespace*
    findNamespace_(const Namespace& nsname);

    SCOCacheNamespace*
    findNamespace_throw_(const Namespace& nsname);

    bool
    hasDisabledNamespace_(const Namespace& nsname);

    SCOCacheNamespace*
    createAndInsertNamespace_(const Namespace& nspace,
                              uint64_t min,
                              uint64_t max_non_disposable);

    void
    removeNamespaceFromMountPoints_(const Namespace& nsname);

    void
    removeNamespaceFromMountPointsInList_(const Namespace& nsname,
                                          const SCOCacheMountPointList&);

    void
    addNamespaceToMountPointsInList_(const Namespace& nsname,
                                     const SCOCacheMountPointList&);

    void
    disableNamespace_(SCOCacheNamespace* ns);

    void
    initMountPoints_();

    void
    removeMountPoint_(const boost::filesystem::path& p,
                      bool force);

    CachedSCOPtr
    createSCO_(const Namespace& nsname,
               SCO scoName,
               uint64_t scoSize,
               float xval,
               bool blocked);

    CachedSCOPtr
    getSCO_(const Namespace& nsname,
            SCO scoName,
            uint64_t scoSize,
            SCOFetcher& fetch,
            float xval,
            bool* cached,
            bool prefetch);

    CachedSCOPtr
    findSCO_(const Namespace& nsname,
             SCO scoName);

    void
    insertSCO_(CachedSCOPtr sco,
               bool blocked);

    void
    removeSCO_(CachedSCOPtr sco,
              bool unlink);

    void
    findAndRemoveSCO_(const Namespace& nsname,
                      SCO scoName,
                      bool removeNonDisposable,
                      bool mustBeBlocked,
                      bool unlink = true);

    void
    initXVals_(SCOCacheNamespace* ns,
               const SCOAccessData& sad);

    float
    getInitialXVal_();

    void
    rescaleXVals_();

    void
    updateXValThresholds_();

    SCOCacheMountPointPtr
    getWriteMountPoint_(uint64_t scoSize);

    void
    reportIOError_(CachedSCOPtr sco);

    void
    offlineMountPoint_(SCOCacheMountPointPtr mp);

    uint64_t
    eraseSCOSetIterator_(SCOSet& scoSet,
                         SCOSet::iterator& it,
                         bool remove_non_disposable);

    void
    ensureNamespaceMin_(SCOCacheNamespace* ns,
                        SCOSet& scoSet,
                        uint64_t totalSize,
                        uint64_t disposableSize);

    void
    maybeChokeNamespaces_();

    typedef std::map<const SCOCacheNamespace*, SCOSet*> NSSCOSets;
    void
    trimMountPoints_(NSSCOSets& nsSCOSets, SCOSet& to_delete);

    void
    trimMountPoint_(const SCOCacheMountPointPtr mp,
                    SCOSet& mpSCOSet,
                    SCOSet& to_delete);

    int
    scoCount_() const;

    bool
    softCacheFull_() const;

    void
    bumpMountPointErrorCount_();

    DECLARE_PARAMETER(trigger_gap);
    DECLARE_PARAMETER(backoff_gap);
    DECLARE_PARAMETER(discount_factor);
    DECLARE_PARAMETER(scocache_mount_points);

public:
    DECLARE_PARAMETER(datastore_throttle_usecs);

};

#define CREATE_SCOCACHE_EXCEPTION(error, desc)  \
    class SCOCache##error##Exception            \
        : public std::exception                 \
{                                               \
public:                                         \
    const char*                                 \
    what() const throw ()                       \
    {                                           \
        return "SCOCacheException: " desc;      \
    }                                           \
}

CREATE_SCOCACHE_EXCEPTION(NoMountPoints, "No mountpoints available");
CREATE_SCOCACHE_EXCEPTION(MountPointIO, "I/O error on mountpoint");

#undef CREATE_SCOCACHE_EXCEPTION

} // namespace volumedriver

#endif // SCO_CACHE_H_

// Local Variables: **
// mode: c++ **
// End: **
