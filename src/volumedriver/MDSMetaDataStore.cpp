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

#include "CachedMetaDataStore.h"
#include "BackendTasks.h"
#include "MDSMetaDataBackend.h"
#include "MDSMetaDataStore.h"
#include "MetaDataStoreBuilder.h"
#include "VolManager.h"
#include "VolumeInterface.h"

#include <youtils/Assert.h>

#include <metadata-server/ClientNG.h>

namespace volumedriver
{

namespace be = backend;
namespace fs = boost::filesystem;
namespace mds = metadata_server;
namespace yt = youtils;

#define LOCKR()                                                 \
    boost::shared_lock<decltype(rwlock_)> rlg__(rwlock_)

#define LOCKW()                                                 \
    boost::unique_lock<decltype(rwlock_)> wlg__(rwlock_)

#define ASSERT_WLOCKED()                                                \
    ASSERT(not rwlock_.try_lock_shared())

MDSMetaDataStore::MDSMetaDataStore(const MDSMetaDataBackendConfig& cfg,
                                   be::BackendInterfacePtr bi,
                                   const fs::path& home,
                                   uint64_t num_pages_cached)
    : VolumeBackPointer(getLogger__())
    , bi_(std::move(bi))
    , node_configs_(cfg.node_configs())
    , apply_relocations_to_slaves_(cfg.apply_relocations_to_slaves())
    , num_pages_cached_(num_pages_cached)
    , home_(home)
{
    check_config_(cfg);

    LOG_INFO(bi_->getNS() <<
             ": home " << home_ <<
             ", cache capacity (pages) " << num_pages_cached_ <<
             ", apply scrub results to slaves: " << apply_relocations_to_slaves_);

    LOG_INFO("MDS nodes:");

    for (const auto& n : node_configs_)
    {
        LOG_INFO("\t" << n);
    }

    VERIFY(not node_configs_.empty());

    LOCKW();

    try
    {
        mdstore_ = connect_(node_configs_[0]);
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR(bi_->getNS() << ": failed to connect to " <<
                      node_configs_[0] << ": " << EWHAT);
            mdstore_ = do_failover_(true);
        });
}

void
MDSMetaDataStore::check_config_(const MDSMetaDataBackendConfig& cfg)
{
    if (cfg.node_configs().empty())
    {
        LOG_ERROR(bi_->getNS() << ": empty list of MDS nodes is not permitted");
        throw fungi::IOException("Empty list of MDS nodes is not permitted");
    }
}

void
MDSMetaDataStore::initialize(VolumeInterface& vol)
{
    LOCKW();
    setVolume(&vol);
}

template<typename R, typename... A>
R
MDSMetaDataStore::handle_(const char* desc,
                          R (MetaDataStoreInterface::*fn)(A... args),
                          A... args)
{
    return do_handle_<R>(desc,
                         [&](const MetaDataStorePtr& md) -> R
        {
            VERIFY(md != nullptr);
            return (md.get()->*fn)(args...);
        });
}

template<typename R, typename F>
R
MDSMetaDataStore::do_handle_(const char* desc,
                             F&& fun)
{
    LOG_TRACE(bi_->getNS() << ": " << desc);

    // we might want to retry more often before kicking off a failover?
    MetaDataStorePtr md;
    try
    {
        LOCKR();

        md = mdstore_;
        if (md != nullptr)
        {
            return fun(md);
        }
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR(bi_->getNS() << ": " << desc << " failed: " << EWHAT);

            failover_(md,
                      desc);

            LOG_INFO(desc << ": retrying");

            LOCKR();

            return fun(md);
        });

    VERIFY(md == nullptr);

    LOG_ERROR(bi_->getNS() <<
              ": no internal MetaDataStore present - all previous MDS failover attempts failed?");
    throw DeadAndGoneException("No internal mdstore present",
                               bi_->getNS().str().c_str());
}

void
MDSMetaDataStore::failover_(MetaDataStorePtr& md,
                            const char* desc)
{
    LOCKW();

    if (md == mdstore_)
    {
        LOG_ERROR(bi_->getNS() << ": " << desc <<
                  ": first thread to see this error - trying to recover");
        try
        {
            md = do_failover_(false);
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR("Failover failed: " << EWHAT);
                mdstore_->drop_cache_including_dirty_pages();
                mdstore_ = nullptr;
                throw;
            });

        mdstore_->drop_cache_including_dirty_pages();
        mdstore_ = md;

        try
        {
            getVolume()->metaDataBackendConfigHasChanged(MDSMetaDataBackendConfig(node_configs_,
                                                                                  apply_relocations_to_slaves_));
        }
        CATCH_STD_ALL_LOG_IGNORE(bi_->getNS() <<
                                 ": notifying our owner of the failover failed");
    }
    else
    {
        LOG_INFO(desc <<
                 " another thread must have taken care of failover already");
        md = mdstore_;
    }
}

MDSMetaDataStore::MetaDataStorePtr
MDSMetaDataStore::do_failover_(bool startup)
{
    ASSERT_WLOCKED();

    VERIFY(not node_configs_.empty());

    // If there's no slave configured we try once more on the master to paper over
    // transient errors.
    const size_t attempts = node_configs_.size() == 1 ? 1 : node_configs_.size() - 1;

    for (size_t i = 0; i < attempts; ++i)
    {
        std::rotate(node_configs_.begin(),
                    node_configs_.begin() + 1,
                    node_configs_.end());

        try
        {
            MetaDataStorePtr md(build_new_one_(node_configs_[0],
                                               startup));
            std::stringstream ss;
            ss << node_configs_[0];

            TODO("AR: getVolume()->getName() instead of using the namespace; but we might not be initialized yet");

            VolumeDriverError::report(events::VolumeDriverErrorCode::MDSFailover,
                                      ss.str(),
                                      VolumeId(bi_->getNS().str()));
            return md;
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR(bi_->getNS() << ": failover to " << node_configs_[0] <<
                          " failed: " << EWHAT);
            });
    }

    LOG_ERROR(bi_->getNS() << ": all failover attempts failed - giving up");
    throw fungi::IOException("All failover attempts failed - giving up");
}

MDSMetaDataStore::MetaDataStorePtr
MDSMetaDataStore::connect_(const MDSNodeConfig& ncfg) const
{
    LOG_INFO(bi_->getNS() << ": connecting to " << ncfg);

    auto mdb(std::make_shared<MDSMetaDataBackend>(ncfg,
                                                  bi_->getNS()));

    auto md(std::make_shared<CachedMetaDataStore>(mdb,
                                                  bi_->getNS().str(),
                                                  num_pages_cached_));

    mdb->set_master();

    return md;
}

MDSMetaDataStore::MetaDataStorePtr
MDSMetaDataStore::build_new_one_(const MDSNodeConfig& ncfg,
                                 bool startup) const
{
    ASSERT_WLOCKED();

    LOG_INFO(bi_->getNS() << ": attempting failover to " << ncfg);

    auto md(connect_(ncfg));

    // mdstore_ can only be nullptr when we ended up here while in the constructor
    // Find a way to enforce this invariant.
    if (not startup)
    {
        VERIFY(mdstore_ != nullptr);

        TODO("AR: consider the case end_cork == boost::none");

        boost::optional<yt::UUID> end_cork(mdstore_->lastCork());

        const fs::path tmpdir(yt::FileUtils::create_temp_dir(home_,
                                                             "failover"));
        MetaDataStoreBuilder build(*md,
                                   bi_->clone(),
                                   tmpdir);
        build(end_cork,
              CheckScrubId::T);

        md->copy_corked_entries_from(*mdstore_);
    }
    else
    {
        VERIFY(mdstore_ == nullptr);
    }

    return md;
}

void
MDSMetaDataStore::readCluster(const ClusterAddress addr,
                              ClusterLocationAndHash& loc)
{
    handle_<void,
            ClusterAddress,
            ClusterLocationAndHash&>(__FUNCTION__,
                                     &MetaDataStoreInterface::readCluster,
                                     addr,
                                     loc);
}

void
MDSMetaDataStore::writeCluster(const ClusterAddress addr,
                               const ClusterLocationAndHash& loc)
{
    handle_<void,
            ClusterAddress,
            decltype(loc)>(__FUNCTION__,
                           &MetaDataStoreInterface::writeCluster,
                           addr,
                           loc);
}

void
MDSMetaDataStore::clear_all_keys()
{
    handle_<void>(__FUNCTION__,
                  &MetaDataStoreInterface::clear_all_keys);
}

void
MDSMetaDataStore::sync()
{
    handle_<void>(__FUNCTION__,
                  &MetaDataStoreInterface::sync);
}

void
MDSMetaDataStore::set_delete_local_artefacts_on_destroy() noexcept
{
    try
    {
        handle_<void>(__FUNCTION__,
                      &MetaDataStoreInterface::set_delete_local_artefacts_on_destroy);
    }
    CATCH_STD_ALL_LOG_IGNORE(bi_->getNS() <<
                             ": failed to request removal of local artefacts on deletion");
}

void
MDSMetaDataStore::set_delete_global_artefacts_on_destroy() noexcept
{
    try
    {
        handle_<void>(__FUNCTION__,
                      &MetaDataStoreInterface::set_delete_global_artefacts_on_destroy);
    }
    CATCH_STD_ALL_LOG_IGNORE(bi_->getNS() <<
                             ": failed to request removal of global artefacts on deletion");

}

void
MDSMetaDataStore::processCloneTLogs(const CloneTLogs& ctl,
                                    const NSIDMap& nsidmap,
                                    const fs::path& tlog_location,
                                    bool sync,
                                    const boost::optional<youtils::UUID>& uuid)
{
    handle_<void,
            decltype(ctl),
            decltype(nsidmap),
            decltype(tlog_location),
            decltype(sync),
            decltype(uuid)>(__FUNCTION__,
                            &MetaDataStoreInterface::processCloneTLogs,
                            ctl,
                            nsidmap,
                            tlog_location,
                            sync,
                            uuid);
}

bool
MDSMetaDataStore::compare(MetaDataStoreInterface& other)
{
    return handle_<bool,
                   decltype(other)>(__FUNCTION__,
                                    &MetaDataStoreInterface::compare,
                                    other);
}

MetaDataStoreFunctor&
MDSMetaDataStore::for_each(MetaDataStoreFunctor& functor,
                           const ClusterAddress max_ca)
{
    return handle_<decltype(functor),
                   decltype(functor),
                   ClusterAddress>(__FUNCTION__,
                                   &MetaDataStoreInterface::for_each,
                                   functor,
                                   max_ca);
}

uint64_t
MDSMetaDataStore::applyRelocs(const std::vector<std::string>& relocs,
                              const NSIDMap& nsid_map,
                              const boost::filesystem::path& tlog_location,
                              SCOCloneID cid,
                              const ScrubId& scrub_id)
{
    TODO("AR: reconsider application of relocations to master tables");

    // Cf. Table.cpp, apply_relocations:
    // A unified apply_relocations via the underlying DataBase/TableInterface
    // to both master and slave tables would be nicer to have. In order to get there
    // we'd however have to drop the local CachedMetaDataStore cache.
    return do_handle_<uint64_t>(__FUNCTION__,
                                [&](const MetaDataStorePtr& md) -> uint64_t
        {
            const uint64_t ret = md->applyRelocs(relocs,
                                                 nsid_map,
                                                 tlog_location,
                                                 cid,
                                                 scrub_id);

            const size_t ncfgs = node_configs_.size();
            if (ncfgs > 1)
            {
                const std::string ns(bi_->getNS().str());

                for (size_t i = 1; i < node_configs_.size(); ++i)
                {
                    try
                    {
                        apply_relocs_on_slave_(node_configs_[i],
                                               relocs,
                                               cid,
                                               scrub_id);
                    }
                    CATCH_STD_ALL_LOG_IGNORE(bi_->getNS() <<
                                             ": failed schedule application of relocs to "
                                             << node_configs_[i]);
                }

                try
                {
                    auto b(new backend_task::Barrier(getVolume()));
                    VolManager::get()->scheduleTask(b);
                }
                CATCH_STD_ALL_LOG_IGNORE(bi_->getNS() <<
                                         ": failed schedule barrier task");
            }

            return ret;
        });
}

void
MDSMetaDataStore::apply_relocs_on_slave_(const MDSNodeConfig& cfg,
                                         const std::vector<std::string>& relocs,
                                         SCOCloneID cid,
                                         const ScrubId& scrub_id)
{
    if (apply_relocations_to_slaves_ == ApplyRelocationsToSlaves::F)
    {
        LOG_INFO("Not applying scrub results to slave " << cfg << " - disabled in config");
    }
    else
    {
        auto f([cfg,
                cid,
                ns = bi_->getNS().str(),
                relocs,
                scrub_id]
               {
                   try
                   {
                       auto client(mds::ClientNG::create(cfg));
                       client->open(ns)->apply_relocations(scrub_id,
                                                           cid,
                                                           relocs);
                   }
                   CATCH_STD_ALL_EWHAT({
                           std::stringstream ss;
                           ss << ns << ": failed to apply relocs to " << cfg <<
                               ": " << EWHAT;
                           LOG_ERROR(ss.str());
                           VolumeDriverError::report(events::VolumeDriverErrorCode::ApplyScrubbingRelocs,
                                                     ss.str(),
                                                     VolumeId(ns));
                       });
               });

        auto t(new backend_task::FunTask(*getVolume(),
                                         std::move(f)));

        VolManager::get()->scheduleTask(t);
    }
}

void
MDSMetaDataStore::getStats(MetaDataStoreStats& stats)
{
    handle_<void,
            decltype(stats)>(__FUNCTION__,
                             &MetaDataStoreInterface::getStats,
                             stats);
}

boost::optional<yt::UUID>
MDSMetaDataStore::lastCork()
{
    return handle_<boost::optional<yt::UUID>>(__FUNCTION__,
                                              &MetaDataStoreInterface::lastCork);
}

void
MDSMetaDataStore::cork(const yt::UUID& cork)
{
    handle_<void,
            decltype(cork)>(__FUNCTION__,
                            &MetaDataStoreInterface::cork,
                            cork);
}

MaybeScrubId
MDSMetaDataStore::scrub_id()
{
    return handle_<MaybeScrubId>(__FUNCTION__,
                                 &MetaDataStoreInterface::scrub_id);
}

void
MDSMetaDataStore::set_scrub_id(const ScrubId& scrub_id)
{
    handle_<void,
            decltype(scrub_id)>(__FUNCTION__,
                                &MetaDataStoreInterface::set_scrub_id,
                                scrub_id);
}

void
MDSMetaDataStore::unCork(const boost::optional<yt::UUID>& cork)
{
    handle_<void,
            decltype(cork)>(__FUNCTION__,
                            &MetaDataStoreInterface::unCork,
                            cork);
}

void
MDSMetaDataStore::set_config(const MDSMetaDataBackendConfig& cfg)
{
    check_config_(cfg);

    const auto& new_configs(cfg.node_configs());

    LOG_INFO(bi_->getNS() << ": new config:");
    LOG_INFO("\tapply scrub results to slaves: " << cfg.apply_relocations_to_slaves());

    for (const auto& n : new_configs)
    {
        LOG_INFO("\t" << n);
    }

    LOCKW();

    VERIFY(not node_configs_.empty());

    if (new_configs[0] != node_configs_[0])
    {
        LOG_INFO(bi_->getNS() << ": " << node_configs_[0] <<
                 " is currently in use, failover to " << new_configs[0] <<
                 " requested");
        mdstore_ = build_new_one_(new_configs[0],
                                  false);
    }
    else
    {
        LOG_INFO(bi_->getNS() << ": " << new_configs[0] <<
                 " is already in use, no need to failover");
    }

    node_configs_ = new_configs;
    apply_relocations_to_slaves_ = cfg.apply_relocations_to_slaves();

#ifndef NDEBUG
    LOG_INFO(bi_->getNS() << ": active config: ");
    LOG_INFO("\tapply scrub results to slaves: " << cfg.apply_relocations_to_slaves());

    for (const auto& n : node_configs_)
    {
        LOG_INFO("\t" << n);
    }
#endif
}

MDSMetaDataBackendConfig
MDSMetaDataStore::get_config() const
{
    LOCKR();
    return MDSMetaDataBackendConfig(node_configs_,
                                    apply_relocations_to_slaves_);
}

MDSNodeConfig
MDSMetaDataStore::current_node() const
{
    LOCKR();
    VERIFY(not node_configs_.empty());
    return node_configs_[0];
}

void
MDSMetaDataStore::updateBackendConfig(const MetaDataBackendConfig& cfg)
{
    try
    {
        const auto& mdb = dynamic_cast<const MDSMetaDataBackendConfig&>(cfg);
        set_config(mdb);
    }
    catch (std::bad_cast& e)
    {
        LOG_ERROR(bi_->getNS() << ": why would anyone want to use a " <<
                  cfg.backend_type() << " config with an MDSMetaDataStore?");
        throw UpdateMetaDataBackendConfigException("Wrong config type - I'm an MDSMetaDataStore!");
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR(bi_->getNS() << ": failed to update metadata backend config: " <<
                      EWHAT);
            throw UpdateMetaDataBackendConfigException(EWHAT);
        });
}

std::unique_ptr<MetaDataBackendConfig>
MDSMetaDataStore::getBackendConfig() const
{
    return std::unique_ptr<MetaDataBackendConfig>(new MDSMetaDataBackendConfig(get_config()));
}

}
