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

#include "DataBase.h"
#include "Table.h"

#include <youtils/SourceOfUncertainty.h>

namespace metadata_server
{

namespace be = backend;
namespace fs = boost::filesystem;
namespace yt = youtils;

#define LOCK()                                          \
    boost::lock_guard<decltype(lock_)> lg__(lock_)

DataBase::DataBase(DataBaseInterfacePtr db,
                   be::BackendConnectionManagerPtr cm,
                   const fs::path& scratch_dir,
                   uint32_t cached_pages,
                   const std::atomic<uint64_t>& poll_secs)
    : db_(db)
    , cm_(cm)
    , scratch_dir_(scratch_dir)
    , cached_pages_(cached_pages)
    , poll_secs_(poll_secs)
{
    VERIFY(cm != nullptr);
    restart_();
}

DataBase::~DataBase()
{
    LOG_INFO("Releasing all open tables (" << tables_.size() << ")");

    LOCK();
    tables_.clear();
}

void
DataBase::restart_()
{
    LOG_INFO("Checking if there's anything to restart");

    LOCK();

    VERIFY(tables_.empty());

    const std::vector<std::string> nspaces(db_->list_namespaces());
    yt::SourceOfUncertainty rand;
    const uint64_t max_ramp_up = poll_secs_.load() * 1000;

    for (const auto& n : nspaces)
    {
        VERIFY(tables_.find(n) == tables_.end());
        const boost::chrono::milliseconds ramp_up(rand(max_ramp_up));

        LOG_INFO("restarting " << n << " with a ramp up of " << ramp_up);

        create_table_(n,
                      ramp_up);
    }
}

TablePtr
DataBase::create_table_(const std::string& nspace,
                             const boost::chrono::milliseconds ramp_up)
{
    auto table(std::make_shared<Table>(db_,
                                       cm_->newBackendInterface(be::Namespace(nspace)),
                                       scratch_dir(nspace),
                                       cached_pages_,
                                       poll_secs_,
                                       ramp_up));
    tables_[nspace] = table;
    return table;
}

TableInterfacePtr
DataBase::open(const std::string& nspace)
{
    // LOG_TRACE("opening " << nspace);

    LOCK();

    TablePtr table;

    auto it = tables_.find(nspace);
    if (it != tables_.end())
    {
        table = it->second;
    }
    else
    {
        table = create_table_(nspace,
                              boost::chrono::milliseconds(0));
    }

    VERIFY(table != nullptr);

    return table;
}

void
DataBase::drop(const std::string& nspace)
{
    LOG_INFO("dropping " << nspace);
    LOCK();

    db_->drop(nspace);
    tables_.erase(nspace);
}

std::vector<std::string>
DataBase::list_namespaces()
{
    LOCK();

    std::vector<std::string> nspaces;
    nspaces.reserve(tables_.size());

    for (const auto& p : tables_)
    {
        nspaces.emplace_back(p.first);
    }

    return nspaces;
}


}
