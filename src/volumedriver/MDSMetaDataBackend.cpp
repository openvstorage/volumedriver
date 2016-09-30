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

#include "CachedMetaDataPage.h"
#include "MDSMetaDataBackend.h"
#include "MDSNodeConfig.h"
#include "VolManager.h"

#include "metadata-server/ClientNG.h"
#include "metadata-server/Manager.h"

#include <youtils/Assert.h>
#include <youtils/Catchers.h>

namespace metadata_server
{

template<>
struct DataBufferTraits<volumedriver::CachePage>
{
    static size_t
    size(const volumedriver::CachePage&)
    {
        return volumedriver::CachePage::size();
    }

    static const void*
    data(const volumedriver::CachePage& page)
    {
        return (const void*)page.data();
    }
};

}

TODO("AR: TODO: introduce batching");

namespace volumedriver
{

using namespace std::string_literals;

namespace be = backend;
namespace mds = metadata_server;
namespace yt = youtils;

namespace
{

const std::string cork_key("cork_id");
const std::string used_clusters_key("used_clusters");
const std::string scrub_id_key("scrub_id");

DECLARE_LOGGER("MDSMetaDataBackendHelpers");

mds::DataBaseInterfacePtr
make_db(const MDSNodeConfig& cfg,
        size_t shmem_size,
        const boost::optional<std::chrono::seconds>& timeout)
{
    mds::DataBaseInterfacePtr db;

    try
    {
        db = VolManager::get()->metadata_server_manager()->find(cfg);
    }
    catch (std::logic_error&)
    {
        LOG_WARN("VolManager not running - this should only happen in a test!");
    }

    if (not db)
    {
        db = mds::ClientNG::create(cfg,
                                   shmem_size,
                                   timeout);
    }
    else
    {
        LOG_INFO(cfg << ": running in-process, using fast path");
    }

    VERIFY(db);

    return db;
}

}

TODO("AR: expose shmem size!?");

MDSMetaDataBackend::MDSMetaDataBackend(const MDSNodeConfig& config,
                                       const be::Namespace& nspace,
                                       const boost::optional<OwnerTag>& owner_tag,
                                       const boost::optional<std::chrono::seconds>& timeout)
try
    : db_(make_db(config,
                  64ULL << 10,
                  timeout))
    , table_(db_->open(nspace.str()))
    , used_clusters_(0)
    , owner_tag_(owner_tag)
{
    LOG_INFO(nspace << ": using " << config << ", owner tag " << owner_tag_);

    init_();
}
CATCH_STD_ALL_LOG_RETHROW(nspace << ": failed to create MDSMetaDataBackend " << config)

MDSMetaDataBackend::MDSMetaDataBackend(mds::DataBaseInterfacePtr db,
                                       const be::Namespace& nspace,
                                       const OwnerTag owner_tag)
try
    : db_(db)
    , table_(db_->open(nspace.str()))
    , used_clusters_(0)
    , owner_tag_(owner_tag)
{
    LOG_INFO(nspace);

    init_();
}
CATCH_STD_ALL_LOG_RETHROW(nspace << ": failed to create MDSMetaDataBackend")

MDSMetaDataBackend::~MDSMetaDataBackend()
{
    LOG_INFO(table_->nspace() << ": used clusters: " << used_clusters_);

    if (T(delete_global_artefacts))
    {
        try
        {
            VERIFY(db_ != nullptr);
            db_->drop(table_->nspace());
        }
        CATCH_STD_ALL_LOG_IGNORE("Failed to clean up");
    }
}

void
MDSMetaDataBackend::init_()
{
    const mds::TableInterface::Keys keys{ mds::Key(used_clusters_key) };
    const mds::TableInterface::MaybeStrings ms(table_->multiget(keys));

    if (ms[0] != boost::none)
    {
        VERIFY(ms[0]->size() == sizeof(used_clusters_));
        used_clusters_ = *(reinterpret_cast<const uint64_t*>(ms[0]->data()));
        LOG_INFO(table_->nspace() << ": used clusters: " << used_clusters_);
    }
}

bool
MDSMetaDataBackend::getPage(CachePage& p)
{
    LOG_TRACE(table_->nspace() <<
              ": page address " << p.page_address());

    const mds::TableInterface::Keys keys{ mds::Key(p.page_address()) };
    const mds::TableInterface::MaybeStrings ms(table_->multiget(keys));

    if (ms[0] != boost::none)
    {
        VERIFY(ms[0]->size() == CachePage::size());
        memcpy(p.data(), ms[0]->data(), ms[0]->size());
        return true;
    }
    else
    {
        return false;
    }
}

void
MDSMetaDataBackend::putPage(const CachePage& p,
                            int32_t used_clusters_delta)
{
    LOG_TRACE(table_->nspace() <<
              ": page address " << p.page_address() <<
              ", used_clusters_delta " <<
              used_clusters_delta);

    const int64_t x = used_clusters_ + used_clusters_delta;
    VERIFY(x >= 0);
    const uint64_t used_clusters = x;

    const mds::TableInterface::Records
        recs{ mds::Record(mds::Key(p.page_address()),
                          mds::Value(p)),
              mds::Record(mds::Key(used_clusters_key),
                          mds::Value(used_clusters)) };

    VERIFY(owner_tag_);

    table_->multiset(recs,
                     Barrier::F,
                     *owner_tag_);

    used_clusters_ = used_clusters;
}

void
MDSMetaDataBackend::discardPage(const CachePage& p,
                                int32_t used_clusters_delta)
{
    LOG_TRACE(table_->nspace() <<
              ": page address " << p.page_address() <<
              ", used_clusters_delta " <<
              used_clusters_delta);

    const int64_t x = used_clusters_ + used_clusters_delta;
    VERIFY(x >= 0);
    const uint64_t used_clusters = x;

    const mds::TableInterface::Records
        recs{ mds::Record(mds::Key(p.page_address()),
                          mds::None()),
              mds::Record(mds::Key(used_clusters_key),
                          mds::Value(used_clusters)) };

    VERIFY(owner_tag_);

    table_->multiset(recs,
                     Barrier::F,
                     *owner_tag_);

    used_clusters_ = used_clusters;
}

void
MDSMetaDataBackend::sync()
{
    LOG_TRACE(table_->nspace());
    TODO("AR: consider what to do here");
}

void
MDSMetaDataBackend::clear_all_keys()
{
    LOG_INFO(table_->nspace() << ": requesting removal of all records");

    VERIFY(owner_tag_);
    table_->clear(*owner_tag_);
}

void
MDSMetaDataBackend::setCork(const yt::UUID& cork_id)
{
    LOG_INFO(table_->nspace() << ": " << cork_id);

    const std::string s(cork_id.str());

    const mds::TableInterface::Records recs{ mds::Record(mds::Key(cork_key),
                                                         mds::Value(s)) };

    VERIFY(owner_tag_);
    table_->multiset(recs,
                     Barrier::T,
                     *owner_tag_);
}

boost::optional<yt::UUID>
MDSMetaDataBackend::lastCorkUUID()
{
    LOG_TRACE(table_->nspace());

    const mds::TableInterface::Keys keys{ mds::Key(cork_key) };
    const mds::TableInterface::MaybeStrings ms(table_->multiget(keys));

    boost::optional<yt::UUID> cork;

    if (ms[0] != boost::none)
    {
        VERIFY(ms[0]->size() == yt::UUID::getUUIDStringSize());
        cork = yt::UUID(*ms[0]);
    }

    LOG_INFO(table_->nspace() << ": " << cork);

    return cork;
}

void
MDSMetaDataBackend::set_scrub_id(const ScrubId& scrub_id)
{
    LOG_INFO(table_->nspace() << ": setting scrub ID " << scrub_id);

    const std::string s(static_cast<const yt::UUID&>(scrub_id).str());
    const mds::TableInterface::Records recs{ mds::Record(mds::Key(scrub_id_key),
                                                         mds::Value(s)) };
    VERIFY(owner_tag_);
    table_->multiset(recs,
                     Barrier::T,
                     *owner_tag_);
}

MaybeScrubId
MDSMetaDataBackend::scrub_id()
{
    LOG_TRACE(table_->nspace());

    const mds::TableInterface::Keys keys{ mds::Key(scrub_id_key) };
    const mds::TableInterface::MaybeStrings ms(table_->multiget(keys));

    MaybeScrubId scrub_id;

    if (ms[0] != boost::none)
    {
        VERIFY(ms[0]->size() == yt::UUID::getUUIDStringSize());
        scrub_id = ScrubId(yt::UUID(*ms[0]));
    }

    LOG_INFO(table_->nspace() << ": scrub ID " << scrub_id);

    return scrub_id;
}

MetaDataStoreFunctor&
MDSMetaDataBackend::for_each(MetaDataStoreFunctor& f,
                             const ClusterAddress ca_max)
{
    // Same implementation as Arakoon's (minus the queue flushing) ... move up?
    const size_t page_size = CachePage::capacity();
    std::vector<ClusterLocationAndHash> clh(page_size);

    for (ClusterAddress ca = 0; ca < ca_max; ca += page_size)
    {
        PageAddress pa = CachePage::pageAddress(ca);

        CachePage p(pa, clh.data());
        bool found = getPage(p);
        if (found)
        {
            for (size_t i = 0; i < page_size; ++i)
            {
                ClusterLocationAndHash loc(p[i]);
                if (not loc.clusterLocation.isNull())
                {
                    f(CachePage::clusterAddress(pa) + i, loc);
                }
            }
        }
    }

    return f;
}

yt::UUID
MDSMetaDataBackend::setFrozenParentCloneCork()
{
    VERIFY(0 == "I'm an MDSMetaDataBackend and don't know how to set a frozen parent clone cork");
}

std::unique_ptr<MetaDataBackendConfig>
MDSMetaDataBackend::getConfig() const
{
    VERIFY(0 == "I'm an MDSMetaDataBackend and you should ask my owner for the config");
}

}
