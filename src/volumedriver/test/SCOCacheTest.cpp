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

#include "../ClusterLocation.h"
#include "../OpenSCO.h"
#include "../SCOFetcher.h"
#include "../TransientException.h"

#include "SCOCacheTestSetup.h"
#include "VolManagerTestSetup.h"

#include <fstream>
#include <iostream>

#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/array.hpp>

#include <backend/BackendConfig.h>
#include <backend/BackendInterface.h>
#include <backend/BackendConnectionManager.h>

#include <youtils/Assert.h>
#include <youtils/FileUtils.h>
#include <youtils/SourceOfUncertainty.h>

namespace volumedriver
{

namespace bio = boost::iostreams;
namespace bpt = boost::property_tree;
namespace bu = boost::uuids;
namespace yt = youtils;

using namespace std::literals::string_literals;
using namespace initialized_params;

class SCOCacheTest
    : public SCOCacheTestSetup
{

public:
    SCOCacheTest()
        : throttling(1000)
    {}

protected:
    DECLARE_LOGGER("SCOCacheTest");

    virtual void
    SetUp()
    {
        SCOCacheTestSetup::SetUp();

        scoSize_ = 4 * 4096;
        triggerGapSCO_ = 2;
        backoffGapSCO_ = 6;
        mpSizeSCO_ = 10;
        triggerGap_ = triggerGapSCO_ * scoSize_;
        backoffGap_ = backoffGapSCO_ * scoSize_;
        mpSize_ = mpSizeSCO_ * scoSize_;

        fs::create_directories(getMountPoint1Path());

        mpcfgs_.push_back(MountPointConfig(getMountPoint1Path(),
                                           mpSizeSCO_ * scoSize_));

        bpt::ptree pt;
        PARAMETER_TYPE(scocache_mount_points)(mpcfgs_).persist(pt);
        PARAMETER_TYPE(datastore_throttle_usecs)(throttling).persist(pt);
        PARAMETER_TYPE(trigger_gap)(yt::DimensionedValue(triggerGap_)).persist(pt);
        PARAMETER_TYPE(backoff_gap)(yt::DimensionedValue(backoffGap_)).persist(pt);
        pt.put("version", 1);
        scoCache_ = std::make_unique<SCOCache>(pt);
    }

    virtual void
    TearDown()
    {
        SCOCacheTestSetup::TearDown();
    }

    std::string
    getMountPoint1Name() const
    {
        return "mp1";
    }

    fs::path
    getMountPoint1Path() const
    {
        return pathPfx_ / getMountPoint1Name();
    }

    void
    addMountPoint(const fs::path& mppath,
                  uint64_t capacity = std::numeric_limits<uint64_t>::max())
    {
        MountPointConfig cfg(mppath,
                             capacity);
        if (not fs::exists(mppath))
        {
            fs::create_directories(mppath);
        }
        scoCache_->addMountPoint(cfg);
    }

    void
    removeMountPoint(const fs::path& mppath)
    {
        scoCache_->removeMountPoint(mppath);
    }

    SCOCacheMountPointList&
    getMountPointList()
    {
        return SCOCacheTestSetup::getMountPointList(*scoCache_);
    }

    uint64_t
    getMountPointErrorCount()
    {
        return SCOCacheTestSetup::getMountPointErrorCount(*scoCache_);
    }

    NSMap&
    getNSMap()
    {
        return SCOCacheTestSetup::getNSMap(*scoCache_);
    }

    void
    addNamespace(const backend::Namespace& nspace,
                 uint64_t min = 0,
                 uint64_t max = std::numeric_limits<uint64_t>::max())
    {
        scoCache_->addNamespace(nspace, min, max);
    }

    void
    removeNamespace(const backend::Namespace& nspace)
    {
        scoCache_->removeNamespace(nspace);
    }

    CachedSCOPtr
    createAndWriteSCO(const backend::Namespace& nspace,
                      SCO scoName,
                      uint64_t scoSize,
                      const std::string& pattern)
    {
        std::vector<byte> buf(scoSize);

        for (size_t i = 0; i < buf.size(); ++i)
        {
            buf[i] = pattern[i % pattern.size()];
        }
        uint32_t throttle;
        CachedSCOPtr sco = scoCache_->createSCO(nspace,
                                                scoName,
                                                scoSize);
        OpenSCOPtr osco(sco->open(FDMode::Write));
        EXPECT_EQ(static_cast<ssize_t>(buf.size()),
                  osco->pwrite(&buf[0], buf.size(), 0, throttle));

        return sco;
    }

    void
    testCleanup(uint64_t min_scos,
                uint64_t max_scos,
                uint64_t scosToWrite,
                uint64_t disposable)
    {
        const backend::Namespace nspace;

        ASSERT_LE(disposable, scosToWrite) << "fix your test";

        addNamespace(nspace,
                     min_scos * scoSize_,
                     max_scos * scoSize_);

        for (unsigned i = 0; i < scosToWrite; ++i)
        {
            ClusterLocation loc(i + 1);
            CachedSCOPtr sco = createAndWriteSCO(nspace,
                                                 loc.sco(),
                                                 scoSize_,
                                                 loc.sco().str());

            if (i < disposable)
            {
                scoCache_->setSCODisposable(sco);
            }
        }

        scoCache_->cleanup();

        uint64_t nonDisposable = scosToWrite - disposable;

        uint64_t scosLeft;

        if (scosToWrite > (mpSizeSCO_ - triggerGapSCO_))
        {
            scosLeft =
                std::max(std::min(scosToWrite,
                                  std::max(nonDisposable,
                                           std::max(min_scos,
                                                    mpSizeSCO_ - backoffGapSCO_))),
                         nonDisposable);
        }
        else
        {
            scosLeft = scosToWrite;
        }

        LOG_DEBUG("scosLeft: " << scosLeft);

        {
            SCONameList l;
            scoCache_->getSCONameList(nspace, l, false);
            EXPECT_EQ(l.size(), nonDisposable) <<
                "non-disposable SCOs must never ever be removed during cleanup";
        }

        {
            SCONameList l;
            scoCache_->getSCONameList(nspace, l, true);
            EXPECT_EQ(l.size(), scosLeft - nonDisposable);
        }

        removeNamespace(nspace);
    }

    void
    offlineMountPoint(SCOCacheMountPointPtr mp)
    {
        fungi::ScopedWriteLock l(scoCache_->rwLock_);
        scoCache_->offlineMountPoint_(mp);
    }

    void
    offlineMountPoint(const fs::path& path)
    {
        for (auto mp : scoCache_->mountPoints_)
        {
            if (mp->getPath() == path)
            {
                offlineMountPoint(mp);
                return;
            }
        }

        FAIL() << "mountpoint " << path << " not present!";
    }

    typedef std::pair<fs::path, unsigned> MPTestConfig;
    typedef std::list<MPTestConfig> MPTestConfigs;

    void
    testBalancing(const MPTestConfigs& mptestcfgs)
    {
        ASSERT_EQ(1U, getMountPointList().size()) << "fix your test";

        unsigned sco_count = mpSizeSCO_;

        for (const auto& c : mptestcfgs)
        {
            addMountPoint(c.first,
                          c.second * scoSize_);
            sco_count += c.second;
        }

        const backend::Namespace nspace;
        addNamespace(nspace);

        for (unsigned i = 0; i < sco_count; ++i)
        {
            std::stringstream ss;
            ss << (i + 1);
            createAndWriteSCO(nspace,
                              SCO(i),
                              scoSize_,
                              ss.str());
            scoCache_->cleanup();
        }

        SCOCacheMountPointList mps(getMountPointList());

        typedef std::map<SCOCacheMountPointPtr, std::list<CachedSCOPtr> > MPSCOs;
        MPSCOs mpscos;

        SCOCacheNamespace* ns = getNSMap()[nspace];

        for (auto& n : *ns)
        {
            SCOCacheNamespaceEntry& e = n.second;
            CachedSCOPtr sco = e.getSCO();

            mpscos[sco->getMountPoint()].push_back(sco);
        }

        EXPECT_EQ(mps.size(), mpscos.size());

        for (auto mp : mps)
        {
            for (const auto& p : mpscos)
            {
                if (p.first == mp)
                {
                    EXPECT_EQ(mp->getUsedSize(), p.second.size() * scoSize_);
                    mpscos[p.first].clear();
                    mpscos.erase(p.first);
                    break;
                }
            }
        }

        EXPECT_EQ(0U, mpscos.size());

        removeNamespace(nspace);

        for (const auto& p : mptestcfgs)
        {
            scoCache_->removeMountPoint(p.first);
            fs::remove_all(p.first);
        }
    }

    void
    persist_configuration(bpt::ptree& pt,
                          ReportDefault report_default)
    {
        scoCache_->persist(pt,
                           report_default);
    }

    MountPointConfigs mpcfgs_;

    uint64_t mpSize_;
    uint64_t mpSizeSCO_;
    uint64_t triggerGapSCO_;
    uint64_t backoffGapSCO_;
    uint64_t triggerGap_;
    uint64_t backoffGap_;
    uint64_t scoSize_;

    std::unique_ptr<SCOCache> scoCache_;
    fs::path backendDir_;

    std::atomic<uint32_t> throttling;
};

TEST_F(SCOCacheTest, namespaces)
{
    const backend::Namespace nspace;

    EXPECT_FALSE(scoCache_->hasNamespace(nspace));

    addNamespace(nspace);

    {
        SCOCacheTestSetup::SCOCacheMountPointList& l = getMountPointList();
        EXPECT_EQ(mpcfgs_.size(), l.size());

        for (auto mp : l)
        {
            EXPECT_TRUE(fs::exists(mp->getPath() / nspace.str()));
        }

        NSMap& m = getNSMap();

        EXPECT_EQ(1U, m.size());
        EXPECT_EQ(nspace, m.begin()->first);
    }

    EXPECT_THROW(addNamespace(nspace),
                 fungi::IOException) <<
        "adding the same namespace a second time must fail";

    EXPECT_TRUE(scoCache_->hasNamespace(nspace));

    removeNamespace(nspace);

    EXPECT_FALSE(scoCache_->hasNamespace(nspace));

    {
        NSMap& m = getNSMap();

        EXPECT_EQ(0U, m.size());

        SCOCacheMountPointList& l = getMountPointList();
        EXPECT_EQ(mpcfgs_.size(), l.size());

        for (auto mp : l)
        {
            EXPECT_FALSE(fs::exists(mp->getPath() / nspace.str()));
        }
    }

    EXPECT_THROW(removeNamespace(nspace),
                 fungi::IOException);

}

TEST_F(SCOCacheTest, NamespaceActivation)
{
    const backend::Namespace nspace;
    addNamespace(nspace);

    for (unsigned i = 0; i < mpSizeSCO_; ++i)
    {
        ClusterLocation loc(i + 1);
        std::stringstream ss;
        ss << loc;
        createAndWriteSCO(nspace,
                          loc.sco(),
                          scoSize_,
                          ss.str());
    }

    SCONameList l;
    scoCache_->getSCONameList(nspace, l, true);
    EXPECT_EQ(0U, l.size());

    scoCache_->getSCONameList(nspace, l, false);
    EXPECT_EQ(mpSizeSCO_, l.size());

    NSMap& nsmap = getNSMap();
    SCOCacheNamespace* ns = nsmap[nspace];

    uint64_t min = ns->getMinSize();
    uint64_t max = ns->getMaxNonDisposableSize();

    SCOAccessData emptySad(nspace);

    EXPECT_THROW(scoCache_->enableNamespace(nspace,
                                            min,
                                            max,
                                            emptySad),
                 fungi::IOException) << "enabling an enabled namespace must fail";

    scoCache_->disableNamespace(nspace);

    EXPECT_THROW(scoCache_->disableNamespace(nspace),
                 fungi::IOException) << "disabling a disabled namespace must fail";

    EXPECT_FALSE(scoCache_->hasNamespace(nspace));

    EXPECT_THROW(scoCache_->findSCO(nspace,
                                    l.front()),
                 std::exception);

    scoCache_->enableNamespace(nspace,
                               min,
                               max,
                               emptySad);

    CachedSCOPtr sco = scoCache_->findSCO(nspace,
                                          l.front());
    EXPECT_TRUE(sco != nullptr);

    l.clear();
    scoCache_->getSCONameList(nspace, l, true);
    EXPECT_EQ(0U, l.size());

    scoCache_->getSCONameList(nspace, l, false);
    EXPECT_EQ(mpSizeSCO_, l.size());
}

TEST_F(SCOCacheTest, scos)
{
    const backend::Namespace nspace;
    addNamespace(nspace);

    const std::vector<ClusterLocation> locs{
        ClusterLocation(1),
        ClusterLocation(2),
    };

    std::vector<byte> buf(4 << 10);

    for (const auto& loc : locs)
    {
        CachedSCOPtr sco = scoCache_->createSCO(nspace,
                                                loc.sco(),
                                                buf.size() * 2);
        EXPECT_FALSE(scoCache_->isSCODisposable(sco));

        {
            OpenSCOPtr osco(sco->open(FDMode::Write));
            uint32_t throttle;
            osco->pwrite(&buf[0], buf.size(), 0, throttle);
            EXPECT_EQ(buf.size() * 2, sco->getSize());
        }

        SCO scoName = sco->getSCO();
        if (scoName.number() % 2 == 0)
        {
            scoCache_->setSCODisposable(sco);
            EXPECT_EQ(buf.size(), sco->getSize());
        }
    }

    {
        SCONameList l;
        scoCache_->getSCONameList(nspace,
                                  l,
                                  false);
        EXPECT_EQ(1U, l.size());
        EXPECT_EQ(locs[0].sco(), l.front());
    }

    {
        SCONameList l;
        scoCache_->getSCONameList(nspace,
                                  l,
                                  true);
        EXPECT_EQ(1U, l.size());
        EXPECT_EQ(locs[1].sco(), l.front());
    }

    ClusterLocation loc3(3);

    EXPECT_TRUE(nullptr == scoCache_->findSCO(nspace,
                                              loc3.sco()));

    EXPECT_THROW(scoCache_->removeSCO(nspace,
                                      locs[0].sco(),
                                      false),
                 fungi::IOException);

    scoCache_->removeSCO(nspace,
                         locs[0].sco(),
                         true);

    scoCache_->removeSCO(nspace,
                         locs[1].sco(),
                         false);

    for (const auto& loc : locs)
    {
        EXPECT_TRUE(nullptr == scoCache_->findSCO(nspace,
                                                  loc.sco()));
    }

    SCONameList l;
    scoCache_->getSCONameList(nspace,
                              l,
                              false);
    EXPECT_EQ(0U, l.size());

    scoCache_->getSCONameList(nspace,
                              l,
                              true);
    EXPECT_EQ(0U, l.size());
}

TEST_F(SCOCacheTest, scosAgain)
{
    auto ns = make_random_namespace();

    const backend::Namespace& nspace = ns->ns();

    addNamespace(nspace);

    ClusterLocation loc(1);
    SCO scoName = loc.sco();
    std::vector<byte> buf(4 << 10);

    {
        CachedSCOPtr sco = scoCache_->createSCO(nspace,
                                                scoName,
                                                buf.size() * 2);
        EXPECT_FALSE(scoCache_->isSCODisposable(sco));

        OpenSCOPtr osco(sco->open(FDMode::Write));
        uint32_t throttle;
        osco->pwrite(&buf[0], buf.size(), 0, throttle);

        EXPECT_EQ(buf.size() * 2, sco->getSize());
    }

    {
        // createTestNamespace(Namespace(nspace));
        BackendInterfacePtr be(createBackendInterface(Namespace(nspace)));

        // look up in cache, put to backend
        {
            CachedSCOPtr sco = scoCache_->findSCO(nspace,
                                                  scoName);

            be->write(sco->path(),
                      scoName.str(),
                      OverwriteObject::F);

            scoCache_->setSCODisposable(sco);
            EXPECT_EQ(buf.size(), sco->getSize());
        }

        // fetch from backend directly
        {
            scoCache_->removeSCO(nspace,
                                 scoName,
                                 false);


            EXPECT_TRUE(nullptr == scoCache_->findSCO(nspace,
                                                      scoName));

            bool cached(true);
            // Z42: don't pass in a NULL-volume, it'll break on errors
            BackendSCOFetcher fetcher(scoName, 0,be->clone());

            CachedSCOPtr sco = scoCache_->getSCO(nspace,
                                                 scoName,
                                                 buf.size() * 2,
                                                 fetcher,
                                                 &cached);

            EXPECT_TRUE(sco != 0);
            EXPECT_TRUE(scoCache_->isSCODisposable(sco));
            EXPECT_EQ(buf.size(), sco->getSize());
            EXPECT_FALSE(cached);
        }

        // fetch from backend via prefetch
        {
            scoCache_->removeSCO(nspace,
                                 scoName,
                                 true);

            EXPECT_TRUE(nullptr == scoCache_->findSCO(nspace,
                                                      scoName));

            // Z42: don't pass in a NULL-volume, it'll break on errors
            BackendSCOFetcher fetcher(scoName, 0, be->clone());
            EXPECT_TRUE(scoCache_->prefetchSCO(nspace,
                                               scoName,
                                               buf.size() * 2,
                                               1.0,
                                               fetcher));

            CachedSCOPtr sco = scoCache_->findSCO(nspace,
                                                  scoName);

            EXPECT_TRUE(sco != 0);
        }
    }

    {
        // not in backend anymore, but only in the cache
        BackendInterfacePtr be(createBackendInterface(Namespace(nspace)));

        bool cached(false);
        // Z42: don't pass in a NULL-volume, it'll break on errors
        BackendSCOFetcher fetcher(scoName, 0, be->clone());
        CachedSCOPtr sco = scoCache_->getSCO(nspace,
                                             scoName,
                                             buf.size() * 2,
                                             fetcher,
                                             &cached);

        EXPECT_TRUE(sco != 0);
        EXPECT_TRUE(cached);
    }
}

TEST_F(SCOCacheTest, cloneSCOs)
{
    auto nsptr1 = make_random_namespace();
    auto nsptr2 = make_random_namespace();

    const backend::Namespace& ns1 = nsptr1->ns();
    const backend::Namespace& ns2 = nsptr2->ns();


    ClusterLocation loc(1);

    const fs::path scrapyard(pathPfx_ / "scrapyard");
    fs::create_directories(scrapyard);

    BackendInterfacePtr be1(createBackendInterface(Namespace(ns1)));
    BackendInterfacePtr be2(createBackendInterface(Namespace(ns2)));

    {
        const fs::path fname(scrapyard / ns1.str());
        std::ofstream f(fname.string().c_str());
        f << fname.leaf().string();
        f.close();

        be1->write(fname.string(),
                   loc.sco().str(),
                   OverwriteObject::F);
    }
    {
        const fs::path fname(scrapyard / ns2.str());
        std::ofstream f(fname.string().c_str());
        f << fname.leaf().string();
        f.close();

        be2->write(fname.string(),
                   loc.sco().str(),
                   OverwriteObject::F);

    }

    fs::remove_all(scrapyard);

    addNamespace(ns1);
    addNamespace(ns2);

    {
        // fetch a SCO from the clone namespace
        bool cached(true);
        SCO sconame = loc.sco();
        // Z42: don't pass in a NULL-volume, it'll break on errors
        BackendSCOFetcher fetcher(sconame, 0, be2->clone());
        CachedSCOPtr sco = scoCache_->getSCO(ns2,
                                             sconame,
                                             1 << 10,
                                             fetcher,
                                             &cached);

        EXPECT_FALSE(cached);
        EXPECT_TRUE(scoCache_->isSCODisposable(sco));

        std::ifstream f(sco->path().string().c_str());
        std::string content;
        f >> content;
        f.close();
        EXPECT_EQ(ns2.str(), content);
    }

    // now the one from the parent namespace

    {
        bool cached(true);
        loc.cloneID(SCOCloneID(1));
        SCO sconame = loc.sco(); //
        // Z42: don't pass in a NULL-volume, it'll break on errors
        BackendSCOFetcher fetcher(sconame,
                                  0,
                                  be1->clone());

        CachedSCOPtr sco = scoCache_->getSCO(ns2,
                                             sconame,
                                             1 << 10,
                                             fetcher,
                                             &cached);

        EXPECT_FALSE(cached);
        EXPECT_TRUE(scoCache_->isSCODisposable(sco));
        std::ifstream f(sco->path().string().c_str());
        std::string content;
        f >> content;
        f.close();
        EXPECT_EQ(ns1.str(), content);
    }
}

TEST_F(SCOCacheTest, restart)
{
    MountPointConfig mp2cfg(pathPfx_ / "mp2",
                            mpSize_);

    fs::create_directories(mp2cfg.path);
    scoCache_->addMountPoint(mp2cfg);

    typedef std::map<std::string, SCONameList> NSSCOs;
    NSSCOs disposables;
    NSSCOs nondisposables;

    unsigned mp_count = getMountPointList().size();

    for (unsigned i = 1; i <= mp_count; ++i)
    {
        TODO("Y42 NON RANDOMIZED NAMESPACE");
        std::stringstream ss;
        ss << "namespace" << i;

        scoCache_->addNamespace(backend::Namespace(ss.str()),
                                0,
                                std::numeric_limits<uint64_t>::max());

        for (unsigned j = 0; j < mpSizeSCO_; ++j)
        {
            ClusterLocation loc(j + 1);
            CachedSCOPtr sco = createAndWriteSCO(backend::Namespace(ss.str()),
                                                 loc.sco(),
                                                 scoSize_,
                                                 ss.str());

            if (j % 2 == 0)
            {
                scoCache_->setSCODisposable(sco);
                disposables[ss.str()].push_back(SCO(j));
            }
            else
            {
                nondisposables[ss.str()].push_back(SCO(j));
            }
        }
    }

    scoCache_.reset();

    mpcfgs_.push_back(mp2cfg);

    bpt::ptree pt;
    PARAMETER_TYPE(datastore_throttle_usecs)(throttling).persist(pt);
    PARAMETER_TYPE(trigger_gap)(yt::DimensionedValue(triggerGap_)).persist(pt);
    PARAMETER_TYPE(backoff_gap)(yt::DimensionedValue(backoffGap_)).persist(pt);
    PARAMETER_TYPE(scocache_mount_points)(mpcfgs_).persist(pt);
    pt.put("version", 1);
    scoCache_.reset(new SCOCache( pt));

    for (unsigned i = 1; i <= mp_count; ++i)
    {
        TODO("Y42: NON RANDOMIZED NAMESPACE");
        std::stringstream ss;
        ss << "namespace" << i;

        SCOAccessData emptySad(backend::Namespace(ss.str()));
        scoCache_->enableNamespace(backend::Namespace(ss.str()),
                                   0,
                                   std::numeric_limits<uint64_t>::max(),
                                   emptySad);

        SCONameList l;
        scoCache_->getSCONameList(backend::Namespace(ss.str()),
                                  l,
                                  false);
        EXPECT_EQ(l.size(), nondisposables[ss.str()].size());

        l.clear();
        scoCache_->getSCONameList(backend::Namespace(ss.str()),
                                  l,
                                  true);
        EXPECT_EQ(l.size(), disposables[ss.str()].size());
    }
}


TEST_F(SCOCacheTest, restartWithMissingMountPoints)
{
    fs::path mp2(pathPfx_ / "mp2");
    MountPointConfig mp2cfg(mp2,
                            mpSize_);
    fs::create_directories(mp2);
    scoCache_->addMountPoint(mp2cfg);

    {
        SCOCacheMountPointList& l = getMountPointList();
        EXPECT_EQ(mpcfgs_.size() + 1, l.size());

        bool found = false;

        for (const auto mp : l)
        {
            if (mp->getPath() == mp2)
            {
                found = true;
                break;
            }
        }

        EXPECT_TRUE(found);
    }

    uint64_t errcnt = getMountPointErrorCount();

    scoCache_.reset();

    fs::path tmp(mp2.string() + ".tmp");
    fs::rename(mp2, tmp);

    bpt::ptree pt;
    pt.put("version", 1);
    PARAMETER_TYPE(datastore_throttle_usecs)(throttling).persist(pt);
    PARAMETER_TYPE(trigger_gap)(yt::DimensionedValue(triggerGap_)).persist(pt);
    PARAMETER_TYPE(backoff_gap)(yt::DimensionedValue(backoffGap_)).persist(pt);
    PARAMETER_TYPE(scocache_mount_points)(mpcfgs_).persist(pt);

    scoCache_.reset(new SCOCache(pt));

    {
        SCOCacheMountPointList& l = getMountPointList();
        EXPECT_EQ(mpcfgs_.size(), l.size());
        EXPECT_EQ(errcnt + 1, getMountPointErrorCount());

        for (const auto mp : l)
        {
            EXPECT_TRUE(mp->getPath() != mp2);
        }
    }

    fs::rename(tmp, mp2);
    EXPECT_THROW(scoCache_->addMountPoint(mp2cfg),
                 std::exception) <<
        "dynamically re-adding an old mountpoint must fail";

    errcnt = getMountPointErrorCount();
    mpcfgs_.push_back(mp2cfg);
    // SCOCache::addMountPointsToPropertyTree(mpcfgs_,
    //                                        pt);
    PARAMETER_TYPE(scocache_mount_points)(mpcfgs_).persist(pt);
    scoCache_.reset(new SCOCache(pt));

    {
        SCOCacheMountPointList& l = getMountPointList();
        EXPECT_EQ(mpcfgs_.size() - 1, l.size());
        EXPECT_EQ(errcnt + 1, getMountPointErrorCount());

        for (const auto mp : l)
        {
            EXPECT_TRUE(mp->getPath() != mp2);
        }
    }
}

TEST_F(SCOCacheTest, addExistingMountPoint)
{
    EXPECT_THROW(addMountPoint(getMountPoint1Path()),
                 fungi::IOException) << "adding existing mountpoints must fail";
}

TEST_F(SCOCacheTest, addExistingMountPointWithSymlink)
{
    fs::path symlink(pathPfx_ / "symlink");
    fs::create_symlink(getMountPoint1Path(), symlink);

    EXPECT_THROW(addMountPoint(symlink),
                 fungi::IOException) <<
        "adding existing mountpoints must fail even when using symlinks";
}

TEST_F(SCOCacheTest, addExistingMountPointWithBlackPathMagic)
{
    fs::path magic(getMountPoint1Path().string() + "/");
    EXPECT_THROW(addMountPoint(magic),
                 fungi::IOException) <<
        "adding existing mountpoint must fail even when messing with the path";
}

TEST_F(SCOCacheTest, addExistingMountPointWithBlackPathMagic2)
{
    fs::path magic(getMountPoint1Path() /
                   std::string("../" + getMountPoint1Path().filename().string()));
    EXPECT_THROW(addMountPoint(magic),
                 fungi::IOException) <<
        "adding existing mountpoint must fail even when messing with the path";
}

TEST_F(SCOCacheTest, addNewMountPointWithInvalidSize)
{
    const fs::path mp2(pathPfx_ / "mp2");

    EXPECT_THROW(addMountPoint(mp2, 0),
                 fungi::IOException) <<
        "capacity 0 must not be accepted";
}

TEST_F(SCOCacheTest, addNewMountPointNonEmpty)
{
    const fs::path mp2(pathPfx_ / "mp2");
    const fs::path dummy(mp2 / "namespacefoo");

    fs::create_directories(dummy);

    EXPECT_THROW(addMountPoint(mp2),
                 fungi::IOException) <<
        "dynamically added mountpoints must be empty";
}

TEST_F(SCOCacheTest, addNewMountPointInexistentPath)
{
    MountPointConfig cfg(pathPfx_ / "mp2",
                         std::numeric_limits<uint64_t>::max());

    EXPECT_TRUE(!fs::exists(cfg.path));
    EXPECT_THROW(scoCache_->addMountPoint(cfg),
                 std::exception) <<
        "dynamically added mountpoint directory must exist";
}

TEST_F(SCOCacheTest, addPreviouslyOfflinedMountPoint)
{
    size_t orig_mp_count = 0;
    {
        SCOCacheMountPointList& l = getMountPointList();
        orig_mp_count = l.size();
    }

    std::vector<fs::path> mps(2);
    for (size_t i = 0; i < mps.size(); ++i)
    {
        std::stringstream ss;
        ss << "mopo" << (i + 1);
        mps[i] = pathPfx_ / ss.str();
        addMountPoint(mps[i]);
    }

    {
        SCOCacheMountPointsInfo info;
        scoCache_->getMountPointsInfo(info);

        EXPECT_EQ(orig_mp_count + mps.size(), info.size());

        for (const auto t : info)
        {
            EXPECT_FALSE(t.second.offlined);
        }
    }

    for (const auto& p : mps)
    {
        offlineMountPoint(p);
    }

    {
        SCOCacheMountPointsInfo info;
        scoCache_->getMountPointsInfo(info);

        EXPECT_EQ(orig_mp_count + mps.size(), info.size());

        for (const auto& t : info)
        {
            if (not t.second.offlined)
            {
                for (const auto& p : mps)
                {
                    EXPECT_TRUE(t.second.path != p);
                }
            }
        }
    }

    for (const auto& p : mps)
    {
        EXPECT_THROW(addMountPoint(p),
                     std::exception);
    }

    {
        SCOCacheMountPointsInfo info;
        scoCache_->getMountPointsInfo(info);

        EXPECT_EQ(orig_mp_count + mps.size(), info.size());

        for (const auto& t : info)
        {
            if (not t.second.offlined)
            {
                for (const auto& p : mps)
                {
                    EXPECT_TRUE(t.second.path != p);
                }
            }
        }
    }

    scoCache_.reset();

    for (const auto& p : mps)
    {
        mpcfgs_.push_back(MountPointConfig(p, std::numeric_limits<uint64_t>::max()));
    }

    bpt::ptree pt;
    pt.put("version", 1);
    PARAMETER_TYPE(datastore_throttle_usecs)(throttling).persist(pt);
    PARAMETER_TYPE(trigger_gap)(yt::DimensionedValue(triggerGap_)).persist(pt);
    PARAMETER_TYPE(backoff_gap)(yt::DimensionedValue(backoffGap_)).persist(pt);
    PARAMETER_TYPE(scocache_mount_points)(mpcfgs_).persist(pt);
    scoCache_.reset(new SCOCache(pt));

    {
        SCOCacheMountPointsInfo info;
        scoCache_->getMountPointsInfo(info);

        EXPECT_EQ(orig_mp_count + mps.size(), info.size());

        for (const auto& t : info)
        {
            if (not t.second.offlined)
            {
                for (const auto& p : mps)
                {
                    EXPECT_TRUE(t.second.path != p);
                }
            }
        }
    }

    {
        SCOCacheMountPointList& l = getMountPointList();
        EXPECT_EQ(orig_mp_count, l.size());
    }

}

TEST_F(SCOCacheTest, addAndRemoveMountPoints)
{
    const backend::Namespace nspace;
    addNamespace(nspace);

    const fs::path mp2(pathPfx_ / "mp2");
    addMountPoint(mp2);

    {
        SCOCacheMountPointList& l = getMountPointList();
        EXPECT_EQ(2U, l.size());
    }

    EXPECT_TRUE(fs::exists(mp2 / nspace.str())) <<
        "existing namespaces must be created on newly added mountpoints";

    EXPECT_NO_THROW(removeMountPoint(getMountPoint1Path())) <<
            "removing a mountpoint must be possible if there are more mountpoints";

    {
        SCOCacheMountPointList& l = getMountPointList();
        EXPECT_EQ(1U, l.size());
        EXPECT_EQ(mp2, l.front()->getPath());
    }

    EXPECT_THROW(removeMountPoint(mp2),
                 fungi::IOException) <<
        "the last mountpoint must not be removable";

    {
        SCOCacheMountPointList& l = getMountPointList();
        EXPECT_EQ(1U, l.size());
        EXPECT_EQ(mp2, l.front()->getPath());
    }
}

TEST_F(SCOCacheTest, cleanup)
{
    SCOCacheMountPointList& l = getMountPointList();
    EXPECT_EQ(1U, l.size());
    SCOCacheMountPointPtr mp = l.front();

    const backend::Namespace nspace;
    uint64_t numSCOs = mpSizeSCO_;

    addNamespace(nspace);

    for (unsigned i = 0; i < numSCOs; ++i)
    {
        ClusterLocation loc(i + 1);
        createAndWriteSCO(nspace,
                          loc.sco(),
                          scoSize_,
                          loc.sco().str());
    }

    {
        ClusterLocation loc(numSCOs + 1);
        EXPECT_THROW(createAndWriteSCO(nspace,
                                       loc.sco(),
                                       scoSize_,
                                       loc.sco().str()),
                     TransientException) <<
            "TransientException must be thrown if capacity would be exceeded";
    }

    EXPECT_EQ(scoSize_* numSCOs, mp->getUsedSize());
    EXPECT_LT(mpSize_ - mp->getUsedSize(), triggerGap_);

    // will not be able to free space since no SCO is disposable
    scoCache_->cleanup();

    {
        SCOCacheMountPointList& l = getMountPointList();
        EXPECT_EQ(1U, l.size());
        SCOCacheMountPointPtr mp = l.front();
        EXPECT_EQ(scoSize_ * numSCOs, mp->getUsedSize()) <<
            "disposable scos cannot be cleaned up";
        EXPECT_LT(triggerGapSCO_ * scoSize_, mp->getUsedSize());
        EXPECT_TRUE(mp->isChoking()) <<
            "mountpoint must be set to 'choking' if it cannot be cleaned up beyond triggergap";
    }

    // increase access probabilities

    //access recent SCO's more often
    for (unsigned i = 1; i <= numSCOs; ++i)
    {
        ClusterLocation loc(i);
        CachedSCOPtr sco = scoCache_->findSCO(nspace,
                                              loc.sco());
        scoCache_->setSCODisposable(sco);
        sco->incRefCount(10 * i);
    }

    {
        SCONameList l;
        scoCache_->getSCONameList(nspace, l, false);
        EXPECT_EQ(0U, l.size());
    }

    scoCache_->cleanup();

    EXPECT_EQ(mp->getUsedSize(), mpSize_ - backoffGap_);

    EXPECT_FALSE(mp->isChoking()) <<
        "mountpoint must not be 'choking' if there's enough free space";

    {
        SCONameList l;
        scoCache_->getSCONameList(nspace, l, true);
        EXPECT_EQ(l.size() * scoSize_, mp->getUsedSize());

        unsigned scosKeptFrom = backoffGapSCO_ + 1;
        for (const auto& scoName : l)
        {
            EXPECT_GE(scoName.number(), scosKeptFrom);
        }
    }
}

TEST_F(SCOCacheTest, cleanup2)
{
    for (uint64_t scomin = 0; scomin <= 12; scomin += 2)
    {
        for (uint64_t scomax = scomin; scomax <= 12; scomax += 2)
        {
            for (uint64_t scoswritten = 1; scoswritten <= 9; scoswritten += 2)
            {
                for (uint64_t disposable = 0; disposable <= scoswritten; disposable++)
                {
                    LOG_DEBUG("scomin: " << scomin << " scoswritten: " <<
                              scoswritten << " disposable: " << disposable);
                    testCleanup(scomin, scomax, scoswritten, disposable);
                }
            }
        }
    }
}

TEST_F(SCOCacheTest, namespaceChoking)
{
    unsigned min_scos = mpSizeSCO_;
    unsigned scos_to_write = mpSizeSCO_ - triggerGapSCO_;
    unsigned max_scos = scos_to_write / 2;

    const backend::Namespace nspace;
    addNamespace(nspace,
                 min_scos * scoSize_,
                 max_scos * scoSize_);

    NSMap& nsmap = getNSMap();
    NSMap::iterator it = nsmap.find(nspace);
    ASSERT_TRUE(it != nsmap.end());
    SCOCacheNamespace* ns = it->second;

    for (unsigned i = 0; i < scos_to_write; ++i)
    {
        scoCache_->cleanup();
        ClusterLocation loc(i + 1);
        CachedSCOPtr sco = createAndWriteSCO(nspace,
                                             loc.sco(),
                                             scoSize_,
                                             loc.sco().str());

        if (i <= max_scos)
        {
            EXPECT_FALSE(ns->isChoking());
        }
        else
        {
            EXPECT_TRUE(ns->isChoking());
        }
    }
}

TEST_F(SCOCacheTest, namespaceChoking2)
{
    unsigned min_scos = mpSizeSCO_;
    unsigned max_scos = 1;

    const backend::Namespace nspace;
    addNamespace(nspace,
                 min_scos * scoSize_,
                 max_scos * scoSize_);

    NSMap& nsmap = getNSMap();
    NSMap::iterator it = nsmap.find(nspace);
    ASSERT_TRUE(it != nsmap.end());
    SCOCacheNamespace* ns = it->second;

    for (unsigned i = 0; i < 2; ++i)
    {
        ClusterLocation loc(i + 1);
        CachedSCOPtr sco = createAndWriteSCO(nspace,
                                             loc.sco(),
                                             scoSize_,
                                             loc.sco().str());
    }

    EXPECT_FALSE(ns->isChoking());
    scoCache_->cleanup();
    EXPECT_TRUE(ns->isChoking());

    for (unsigned i = 0; i < 2; ++i)
    {
        ClusterLocation loc(i + 1);
        CachedSCOPtr sco = scoCache_->findSCO(nspace,
                                              loc.sco());
        scoCache_->setSCODisposable(sco);
    }

    scoCache_->cleanup();
    EXPECT_FALSE(ns->isChoking());
}

TEST_F(SCOCacheTest, SCOBalancing)
{
    MPTestConfigs cfgs;

    for (unsigned mpcount = 2; mpcount < 4; ++mpcount)
    {
        std::stringstream ss;
        ss << "mp" << mpcount;

        MPTestConfig cfg(fs::path(pathPfx_ / ss.str()),
                         mpSizeSCO_);

        cfgs.push_back(cfg);

        testBalancing(cfgs);
    }
}

TEST_F(SCOCacheTest, SCOBalancing2)
{
    MPTestConfigs cfgs;

    for (unsigned mpcount = 2; mpcount < 7; ++mpcount)
    {
        std::stringstream ss;
        ss << "mp" << mpcount;

        MPTestConfig cfg(fs::path(pathPfx_ / ss.str()),
                         mpSizeSCO_ * mpcount);

        cfgs.push_back(cfg);

        testBalancing(cfgs);
    }
}

TEST_F(SCOCacheTest, SAPs)
{
    EXPECT_GE(backoffGapSCO_, mpSizeSCO_ / 2) << "fix this test";
    unsigned num_fills = 3;

    const backend::Namespace nspace;
    addNamespace(nspace);

    unsigned sconum = 1;

    for (unsigned i = 1; i <= num_fills; ++i)
    {
        for (unsigned j = 0; j < mpSizeSCO_; ++j)
        {
            SCO loc(sconum++);
            {
                CachedSCOPtr sco =
                    createAndWriteSCO(nspace,
                                      loc,
                                      scoSize_,
                                      loc.str());
                scoCache_->setSCODisposable(sco);
            }

            if (getMountPointList().front()->getUsedSize() == mpSizeSCO_ * scoSize_)
            {
                for (unsigned k = 1; k <= (mpSizeSCO_ - backoffGapSCO_); ++k)
                {
                    CachedSCOPtr sco =
                        scoCache_->findSCO(nspace,
                                           SCO(k));
                    for (int l = 0; l < 100; ++l)
                    {
                        sco->incRefCount(1);
                    }
                }

                scoCache_->cleanup();
            }
        }
    }

    SCONameList l;
    scoCache_->getSCONameList(nspace, l, false);
    EXPECT_EQ(0U, l.size());

    scoCache_->getSCONameList(nspace, l, true);

    l.sort();

    sconum = 1;
    for (SCONameList::const_iterator it = l.begin();
         it != l.end() && (sconum <= mpSizeSCO_ - backoffGapSCO_);
         ++it)
    {
        EXPECT_EQ(*it, SCO(sconum++));
    }
}

TEST_F(SCOCacheTest, MountPointsInfo)
{
    addMountPoint(pathPfx_ / "mp2", mpSize_);

    const backend::Namespace nspace;
    addNamespace(nspace);

    uint64_t disposable = 0;
    uint64_t nondisposable = 0;
    int num_scos = 7;

    for (int i = 0; i < num_scos; ++i)
    {
        ClusterLocation loc(i + 1);
        CachedSCOPtr sco = createAndWriteSCO(nspace,
                                             loc.sco(),
                                             scoSize_,
                                             loc.sco().str());

        if (i % 2 == 0)
        {
            scoCache_->setSCODisposable(sco);
            disposable += scoSize_;
        }
        else
        {
            nondisposable += scoSize_;
        }
    }

    SCOCacheMountPointsInfo info;
    scoCache_->getMountPointsInfo(info);
    EXPECT_EQ(2U, info.size());

    uint64_t used = 0;

    for (const auto& p : info)
    {
        EXPECT_EQ(p.first, p.second.path);
        const SCOCacheMountPointInfo& mpinfo = p.second;
        EXPECT_EQ(0U, mpinfo.used % scoSize_);
        used += mpinfo.used;
    }

    EXPECT_EQ(used, disposable + nondisposable);

    SCOCacheNamespaceInfo nsinfo = scoCache_->getNamespaceInfo(nspace);
    EXPECT_EQ(nsinfo.disposable, disposable);
    EXPECT_EQ(nsinfo.nondisposable, nondisposable);

}

TEST_F(SCOCacheTest, NamespacesInfo)
{
    std::vector<backend::Namespace> nspaces;

    for (int i = 1; i < 5; ++i)
    {
        std::stringstream ss;
        ss << "nspace" << i;

        addNamespace(backend::Namespace(ss.str()));
        nspaces.emplace_back(ss.str());

        for (int j = 1; j < 3; ++j)
        {
            ClusterLocation loc(j);
            CachedSCOPtr sco = createAndWriteSCO(backend::Namespace(ss.str()),
                                                 loc.sco(),
                                                 scoSize_,
                                                 loc.sco().str());

            if (j % 2 == 0)
            {
                scoCache_->setSCODisposable(sco);
            }
        }
    }

    for (const auto& ns : nspaces)
    {
        SCOCacheNamespaceInfo info = scoCache_->getNamespaceInfo(ns);
        EXPECT_EQ(scoSize_, info.disposable);
        EXPECT_EQ(scoSize_, info.nondisposable);
    }
}

TEST_F(SCOCacheTest, offlineMountPoint)
{
    const fs::path mp2(pathPfx_ / "mp2");
    addMountPoint(mp2);

    uint64_t errcnt;
    {
        SCOCacheMountPointList& l = getMountPointList();
        EXPECT_EQ(2U, l.size());

        SCOCacheMountPointPtr mp1 = l.front();
        SCOCacheMountPointPtr mp2 = l.back();

        errcnt = getMountPointErrorCount();
        EXPECT_EQ(1U, errcnt);
        EXPECT_EQ(errcnt, mp1->getErrorCount());
        EXPECT_EQ(errcnt, mp2->getErrorCount());
    }

    const backend::Namespace nspace;
    addNamespace(nspace);

    for (int i = 0; i < 4; ++i)
    {
        ClusterLocation loc(i + 1);
        CachedSCOPtr sco = createAndWriteSCO(nspace,
                                             loc.sco(),
                                             scoSize_,
                                             loc.sco().str());
        scoCache_->setSCODisposable(sco);
    }

    SCONameList doomedSCOs;
    SCONameList survivingSCOs;

    CachedSCOPtr sco =
        scoCache_->findSCO(nspace,
                           SCO(1));

    SCOCacheMountPointPtr brokenmp = sco->getMountPoint();

    {
        NSMap& nsmap = getNSMap();
        SCOCacheNamespace* ns = nsmap.find(nspace)->second;

        for (auto& p : *ns)
        {
            CachedSCOPtr sco = p.second.getSCO();
            if (sco->getMountPoint() == brokenmp)
            {
                doomedSCOs.push_back(sco->getSCO());
            }
            else
            {
                survivingSCOs.push_back(sco->getSCO());
            }
        }
    }

    // since we have refcounted pointers to the sco and the mountpoint we
    // should try to report errors on that sco/mountpoint multiple times b/c this
    // might happen "in the field".
    for (int i = 0; i < 3; ++i)
    {
        scoCache_->reportIOError(sco);

        {
            SCOCacheMountPointList& l = getMountPointList();
            EXPECT_EQ(1U, l.size());
            EXPECT_TRUE(l.front() != brokenmp);
            EXPECT_EQ(errcnt + 1, l.front()->getErrorCount());
            EXPECT_EQ(errcnt, brokenmp->getErrorCount());

            for (const auto& n : survivingSCOs)
            {
                EXPECT_TRUE(nullptr != scoCache_->findSCO(nspace,
                                                          n));
            }

            for (const auto& n : doomedSCOs)
            {
                EXPECT_TRUE(nullptr == scoCache_->findSCO(nspace,
                                                          n));
            }
        }
    }
}

TEST_F(SCOCacheTest, removeDisabledNamespace)
{
    const backend::Namespace ns;

    scoCache_->addNamespace(ns, 0, std::numeric_limits<uint64_t>::max());
    addMountPoint(pathPfx_ / "mp2");

    uint64_t scosize = 1 << 10;
    size_t scocount = 32;

    std::list<fs::path> paths;

    for (size_t i = 0; i < scocount; ++i)
    {
        SCO sco(i + 1);
        CachedSCOPtr scoptr = createAndWriteSCO(ns,
                                                SCO(i + 1),
                                                scosize,
                                                sco.str());
        paths.push_back(scoptr->path());
    }

    EXPECT_THROW(scoCache_->removeDisabledNamespace(ns),
                 std::exception);

    SCONameList l;
    scoCache_->getSCONameListAll(ns, l);
    EXPECT_EQ(scocount, l.size());

    for (const auto& p : paths)
    {
        EXPECT_TRUE(fs::exists(p));
    }

    scoCache_->disableNamespace(ns);
    EXPECT_TRUE(scoCache_->hasDisabledNamespace(ns));

    scoCache_->removeDisabledNamespace(ns);
    EXPECT_FALSE(scoCache_->hasDisabledNamespace(ns));

    for (const auto& p : paths)
    {
        EXPECT_FALSE(fs::exists(p));
    }
}

TEST_F(SCOCacheTest, recursiveOffliningDuringCleanup)
{
    addMountPoint(pathPfx_ / "mp2");
    const std::string suffix(".broken");

    {
        SCOCacheMountPointsInfo info;
        scoCache_->getMountPointsInfo(info);
        EXPECT_EQ(2U, info.size());

        for (const auto& v : info)
        {
            ASSERT_FALSE(v.second.offlined);
            const fs::path tmp(v.first.string() + suffix);
            fs::rename(v.first, tmp);
        }
    }

    EXPECT_NO_THROW(scoCache_->cleanup());

    {
        SCOCacheMountPointsInfo info;
        scoCache_->getMountPointsInfo(info);
        EXPECT_EQ(2U, info.size());

        for (const auto& v : info)
        {
            ASSERT_TRUE(v.second.offlined);
        }
    }

    addMountPoint(pathPfx_ / "mp3");

    const backend::Namespace nspace;
    addNamespace(nspace);

    for (size_t i = 0; i < 3; ++i)
    {
        ClusterLocation loc(i + 1);
        CachedSCOPtr sco(createAndWriteSCO(nspace,
                                           loc.sco(),
                                           scoSize_,
                                           loc.sco().str()));
    }

    {
        SCOCacheMountPointsInfo info;
        scoCache_->getMountPointsInfo(info);
        EXPECT_EQ(3U, info.size());

        for (const auto& v : info)
        {
            if (v.first == fs::path(pathPfx_ / "mp3"))
            {
                ASSERT_FALSE(v.second.offlined);
            }
            else
            {
                ASSERT_TRUE(v.second.offlined);
            }
        }
    }
}

// OVS-2750
TEST_F(SCOCacheTest, restart_after_offlining)
{
    const std::vector<fs::path> paths{ getMountPoint1Path(),
                                       pathPfx_ / "mp2" };

    fs::create_directories(paths[1]);

    const MountPointConfig mp2cfg(paths[1],
                                  mpSize_);
    scoCache_->addMountPoint(mp2cfg);

    bpt::ptree pt;

    persist_configuration(pt,
                          ReportDefault::T);

    SCOCacheMountPointsInfo info_old;
    scoCache_->getMountPointsInfo(info_old);

    EXPECT_EQ(2U,
              info_old.size());

    for (const auto& v : info_old)
    {
        ASSERT_TRUE(v.first == paths[0] or
                    v.first == paths[1]);

        EXPECT_FALSE(v.second.offlined);
    }

    yt::SourceOfUncertainty sou;
    const unsigned idx = sou(0, 1);

    offlineMountPoint(paths[idx]);

    auto check([&]
               {
                   SCOCacheMountPointsInfo info;
                   scoCache_->getMountPointsInfo(info);

                   for (const auto& v : info)
                   {
                       if (v.first == paths[idx])
                       {
                           EXPECT_TRUE(v.second.offlined);
                       }
                       else
                       {
                           EXPECT_FALSE(v.second.offlined);
                       }
                   }
               });

    check();

    scoCache_.reset();
    scoCache_ = std::make_unique<SCOCache>(pt);

    check();
}

// OVS-2750
TEST_F(SCOCacheTest, restart_after_offlining_a_recently_added_mountpoint)
{
    bpt::ptree pt;
    persist_configuration(pt,
                          ReportDefault::T);

    // the voldrv was first running with only a single mountpoint and
    // restarted several times with it before the second one was added.

    uint64_t mpcounter = 13;

    for (size_t i = 0; i < mpcounter; ++i)
    {
        scoCache_.reset();
        scoCache_ = std::make_unique<SCOCache>(pt);
    }

    {
        ++mpcounter; // the initial startup set it to 1
        SCOCacheMountPointList& l = getMountPointList();
        EXPECT_EQ(1U, l.size());
        EXPECT_EQ(mpcounter,
                  l.front()->getErrorCount());
    }

    const std::vector<fs::path> paths{ getMountPoint1Path(),
                                       pathPfx_ / "mp2" };

    fs::create_directories(paths[1]);

    persist_configuration(pt,
                          ReportDefault::T);

    {
        PARAMETER_TYPE(scocache_mount_points)(smps_old)(pt);
        MountPointConfigs mpcfgs(smps_old.value());
        mpcfgs.emplace_back(MountPointConfig(paths[1],
                                             mpSize_));
        PARAMETER_TYPE(scocache_mount_points)(smps_new)(mpcfgs);
        smps_new.persist(pt);
    }

    scoCache_.reset();
    scoCache_ = std::make_unique<SCOCache>(pt);

    ++mpcounter; // we restarted.

    SCOCacheMountPointsInfo info_old;
    scoCache_->getMountPointsInfo(info_old);

    EXPECT_EQ(2U,
              info_old.size());

    for (const auto& v : info_old)
    {
        ASSERT_TRUE(v.first == paths[0] or
                    v.first == paths[1]);

        EXPECT_FALSE(v.second.offlined);
    }

    offlineMountPoint(paths[1]);

    ++mpcounter; // offlining bumped it.

    auto check([&]
               {
                   SCOCacheMountPointsInfo info;
                   scoCache_->getMountPointsInfo(info);

                   for (const auto& v : info)
                   {
                       if (v.first == paths[1])
                       {
                           EXPECT_TRUE(v.second.offlined);
                       }
                       else
                       {
                           EXPECT_FALSE(v.second.offlined);
                       }
                   }

                   SCOCacheMountPointList& l = getMountPointList();
                   for (const auto& mp : l)
                   {
                       if (mp->getPath() == paths[1])
                       {
                           EXPECT_NE(mpcounter,
                                     mp->getErrorCount());
                       }
                       else
                       {
                           EXPECT_EQ(mpcounter,
                                     mp->getErrorCount());
                       }
                   }
               });

    check();

    scoCache_.reset();
    scoCache_ = std::make_unique<SCOCache>(pt);

    ++mpcounter; // restart bumped it.

    check();
}

namespace
{
struct MPInfo
{
    bu::uuid uuid;
    uint64_t count;

    template<typename A>
    void
    serialize(A& ar,
              unsigned /* version */)
    {
        for (auto& u : uuid)
        {
            ar & u;
        }

        ar & count;
    }
};

}

TEST_F(SCOCacheTest, DISABLED_ovs_2750)
{
    const std::vector<uint8_t> cereal_1 = {
        0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0x65, 0x72, 0x69,
        0x61, 0x6c, 0x69, 0x7a, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x3a, 0x3a, 0x61,
        0x72, 0x63, 0x68, 0x69, 0x76, 0x65, 0x0b, 0x00, 0x04, 0x08, 0x04, 0x08,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x19, 0x8d,
        0xc7, 0x15, 0xd7, 0x45, 0x99, 0x93, 0x1f, 0xeb, 0x72, 0x1e, 0xcd, 0x27,
        0xdf, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    const std::vector<uint8_t> cereal_2 = {
        0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0x65, 0x72, 0x69,
        0x61, 0x6c, 0x69, 0x7a, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x3a, 0x3a, 0x61,
        0x72, 0x63, 0x68, 0x69, 0x76, 0x65, 0x0b, 0x00, 0x04, 0x08, 0x04, 0x08,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x89, 0x38,
        0x03, 0xd5, 0x9d, 0x46, 0xd7, 0xb4, 0xf8, 0x31, 0x24, 0xf5, 0x10, 0x36,
        0xb3, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    auto print([](const std::vector<uint8_t>& vec,
                  const char* desc)
               {
                   bio::stream<bio::array_source>
                       is(reinterpret_cast<const char*>(vec.data()),
                          vec.size());

                   boost::archive::binary_iarchive ia(is);
                   MPInfo mpinfo;
                   ia >> mpinfo;

                   std::cout << desc <<
                       ": uuid " << mpinfo.uuid <<
                       ", count " << mpinfo.count << std::endl;
               });

    print(cereal_1,
          "MP1");

    print(cereal_2,
          "MP2");
}

TEST_F(SCOCacheTest, accounting_of_disabled_namespaces)
{
    const backend::Namespace ns1("nspace-1"s);
    addNamespace(ns1);

    auto check_size([&](size_t exp)
                    {
                        size_t size = 0;

                        SCOCacheMountPointsInfo info;
                        scoCache_->getMountPointsInfo(info);
                        for (const auto& i : info)
                        {
                            size += i.second.used;
                        }

                        EXPECT_EQ(exp, size);
                    });

    check_size(0);

    const size_t ns1_sco_count = mpSizeSCO_ - 1;

    for (unsigned i = 0; i < ns1_sco_count; ++i)
    {
        createAndWriteSCO(ns1,
                          SCO(i + 1),
                          scoSize_,
                          ns1.str() + boost::lexical_cast<std::string>(i));
    }

    check_size(ns1_sco_count * scoSize_);

    bpt::ptree pt;
    persist_configuration(pt,
                          ReportDefault::T);

    scoCache_.reset();
    scoCache_ = std::make_unique<SCOCache>(pt);

    check_size(ns1_sco_count * scoSize_);

    const backend::Namespace ns2("nspace-2"s);
    addNamespace(ns2);

    createAndWriteSCO(ns2,
                      SCO(1),
                      scoSize_,
                      ns1.str() + "1"s);

    check_size(mpSizeSCO_ * scoSize_);

    EXPECT_THROW(scoCache_->createSCO(ns2,
                                      SCO(2),
                                      scoSize_),
                 fungi::IOException);

    check_size(mpSizeSCO_ * scoSize_);

    SCOAccessData emptySad(ns1);

    EXPECT_NO_THROW(scoCache_->enableNamespace(ns1,
                                               0,
                                               std::numeric_limits<uint64_t>::max(),
                                               emptySad));

    check_size(mpSizeSCO_ * scoSize_);

    EXPECT_NO_THROW(scoCache_->disableNamespace(ns1));

    check_size(mpSizeSCO_ * scoSize_);
}

}

// Local Variables: **
// mode: c++ **
// End: **
