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
        size_t shmem_size)
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
                                   shmem_size);
    }
    else
    {
        LOG_INFO(cfg << ": running in-process, using fast path");
    }

    VERIFY(db);

    return db;
}

}

TODO("AR: add a timeout param and expose shmem size!");

MDSMetaDataBackend::MDSMetaDataBackend(const MDSNodeConfig& config,
                                       const be::Namespace& nspace)
try
    : db_(make_db(config,
                  64ULL << 10))
    , table_(db_->open(nspace.str()))
    , used_clusters_(0)
{
    LOG_INFO(nspace << ": using " << config);

    init_();
}
CATCH_STD_ALL_LOG_RETHROW(nspace << ": failed to create MDSMetaDataBackend " << config)

MDSMetaDataBackend::MDSMetaDataBackend(mds::DataBaseInterfacePtr db,
                                       const be::Namespace& nspace)
try
    : db_(db)
    , table_(db_->open(nspace.str()))
    , used_clusters_(0)
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

    table_->multiset(recs,
                     Barrier::F);

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

    table_->multiset(recs,
                     Barrier::F);

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
    table_->clear();
}

void
MDSMetaDataBackend::setCork(const yt::UUID& cork_id)
{
    LOG_INFO(table_->nspace() << ": " << cork_id);

    const std::string s(cork_id.str());

    const mds::TableInterface::Records recs{ mds::Record(mds::Key(cork_key),
                                                         mds::Value(s)) };

    table_->multiset(recs,
                     Barrier::T);
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
    table_->multiset(recs,
                     Barrier::T);
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
