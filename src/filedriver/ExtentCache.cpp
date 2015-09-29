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

#include "ExtentCache.h"

namespace filedriver
{

namespace fs = boost::filesystem;

ExtentCache::ExtentCache(const boost::filesystem::path& path,
                         uint32_t capacity)
    : path_(path)
    , cache_("FileDriverExtentCache",
             capacity,
             [&](const Cache::Key& eid,
                 const Cache::Value&)
             {
                 evict_extent_from_cache_(eid);
             })
{
    if (not fs::exists(path_))
    {
        LOG_ERROR("Cache dir " << path_ << " does not exist");
        throw fungi::IOException("Extent cache directory does not exist",
                                 path_.string().c_str(),
                                 ENOENT);
    }

    if (not fs::is_directory(path_))
    {
        LOG_ERROR(path_ << " is not a directory");
        throw fungi::IOException("Specified path for extent cache is not a directory",
                                 path_.string().c_str(),
                                 ENOENT);
    }

    // XXX: support restarting with a warm cache?
    for (auto it = fs::directory_iterator(path_);
         it != fs::directory_iterator();
         ++it)
    {
        LOG_WARN("Leftover entry " << *it << " in extent cache - removing it");
        fs::remove_all(*it);
    }
}

fs::path
ExtentCache::make_path_(const Cache::Key& eid)
{
    LOG_TRACE(eid);
    return path_ / eid.str();
}

void
ExtentCache::evict_extent_from_cache_(const Cache::Key& eid)
{
    LOG_INFO("removing extent " << eid << " from cache");
    fs::remove_all(make_path_(eid));
}

std::shared_ptr<Extent>
ExtentCache::find(const ExtentId& eid,
                  PullFun&& fn)
{
    return cache_.find(eid,
                       [&](const Cache::Key& eid)
                       {
                           return fn(eid, make_path_(eid));
                       });
}

}
