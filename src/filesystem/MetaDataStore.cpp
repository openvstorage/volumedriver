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

#include "FileSystem.h"
#include "MetaDataStore.h"

#include <stack>

#include <sys/stat.h>

#include <youtils/Weed.h>

namespace volumedriverfs
{

namespace ara = arakoon;
namespace yt = youtils;

#define HARAPATH(p)                             \
    MetaDataStoreKeyTraits<FrontendPath>::make_key(p)

#define RLOCK_CACHE()                                                   \
    boost::shared_lock<decltype(cache_lock_)> sclg__(cache_lock_)

#define WLOCK_CACHE()                                                   \
    boost::unique_lock<decltype(cache_lock_)> uclg__(cache_lock_)

namespace
{

// XXX: offer something like umask(2) instead?
const Permissions
default_directory_permissions(S_IWUSR bitor S_IRUSR bitor S_IXUSR bitor
                              S_IWGRP bitor S_IRGRP bitor S_IXGRP bitor
                              S_IROTH bitor S_IXOTH);

std::string
make_harakoon_prefix(const ClusterId& cid)
{
    return cid.str() + "/meta";
}

}

MetaDataStore::MetaDataStore(std::shared_ptr<yt::LockedArakoon> larakoon,
                             const ClusterId& cid,
                             UseCache use_cache)
    : harakoon_(larakoon, make_harakoon_prefix(cid))
    , inode_alloc_(cid, larakoon)
    , use_cache_(use_cache)
{
    maybe_initialise_();
}

void
MetaDataStore::destroy(std::shared_ptr<yt::LockedArakoon> larakoon,
                       const ClusterId& cid)
{
    LOG_INFO(cid << ": destroying MetaDataStore");

    try
    {
        HierarchicalArakoon::destroy(larakoon, make_harakoon_prefix(cid));
    }
    CATCH_STD_ALL_LOG_IGNORE(cid << ": failed to destroy HierarchicalArakoon");

    try
    {
        InodeAllocator(cid, larakoon).destroy();
    }
    CATCH_STD_ALL_LOG_IGNORE(cid << ": failed to destroy InodeAllocator");
}

void
MetaDataStore::maybe_initialise_()
{
    if (not harakoon_.initialized())
    {
        LOG_INFO("Trying to initialize filesystem metadata store");
        try
        {
            harakoon_.initialize(DirectoryEntry(DirectoryEntry::Type::Directory,
                                                alloc_inode(),
                                                default_directory_permissions,
                                                UserId(::getuid()),
                                                GroupId(::getgid())));
            LOG_INFO("Initialized filesystem metadata store");
        }
        catch (ara::error_assertion_failed&)
        {
            // Another cluster node could've beaten us to it:
            if (not harakoon_.initialized())
            {
                throw;
            }
            else
            {
                LOG_INFO("Filesystem metadata store already initialized by another node");
            }
        }
    }
    else
    {
        LOG_INFO("Filesystem metadata store already initialized");
    }
}

void
MetaDataStore::maybe_add_to_cache_(const FrontendPath& p,
                                   DirectoryEntryPtr& dentry)
{
    LOG_TRACE(p << ", object id " << dentry->object_id());

    if (use_cache_ == UseCache::T and
        dentry->type() == DirectoryEntry::Type::Volume)
    {
        LOG_TRACE(p);

        WLOCK_CACHE();
        /* Should insert_or_modify perform much better here?
         * For at least a small number of entries (<2048)
         * */
        bimap_cache_.left.erase(p);
        bimap_cache_.insert(cache_entry_t(p, dentry->object_id(), dentry));
    }
}

void
MetaDataStore::maybe_add_to_cache_(const ObjectId& id,
                                   DirectoryEntryPtr& dentry)
{
    if (use_cache_ == UseCache::T)
    {
        const FrontendPath p(find_path(id));
        maybe_add_to_cache_(p, dentry);
    }
}

DirectoryEntryPtr
MetaDataStore::find_in_cache_(const FrontendPath& p)
{
    LOG_TRACE(p);

    RLOCK_CACHE();

    if (use_cache_ == UseCache::T)
    {
        auto it = bimap_cache_.left.find(p);
        if (it != bimap_cache_.left.end())
        {
            LOG_TRACE("Cache hit: " << p);
            return it->info;
        }
        else
        {
            LOG_TRACE("Cache miss: " << p);
            return nullptr;
        }
    }
    else
    {
        VERIFY(bimap_cache_.left.empty());
        return nullptr;
    }
}

DirectoryEntryPtr
MetaDataStore::find_in_cache_(const ObjectId& id)
{
    LOG_TRACE(id);

    RLOCK_CACHE();

    if (use_cache_ == UseCache::T)
    {
        auto it = bimap_cache_.right.find(id);
        if (it != bimap_cache_.right.end())
        {
            LOG_TRACE("Cache hit: " << id);
            return it->info;
        }
        else
        {
            LOG_TRACE("Cache miss: " << id);
            return nullptr;
        }
    }
    else
    {
        VERIFY(bimap_cache_.right.empty());
        return nullptr;
    }
}

void
MetaDataStore::drop_from_cache(const FrontendPath& p)
{
    LOG_TRACE(p);

    WLOCK_CACHE();

    if (use_cache_ == UseCache::T)
    {
        bimap_cache_.left.erase(p);
    }
    else
    {
        VERIFY(bimap_cache_.left.empty());
    }
}

void
MetaDataStore::drop_from_cache(const ObjectId& id)
{
    LOG_TRACE(id);

    WLOCK_CACHE();

    if (use_cache_ == UseCache::T)
    {
        bimap_cache_.right.erase(id);
    }
    else
    {
        VERIFY(bimap_cache_.left.empty());
    }
}

FrontendPath
MetaDataStore::find_path(const ObjectId& id)
{
    return FrontendPath(harakoon_.get_path_by_id(yt::UUID(id.str())).str());
}

void
MetaDataStore::walk(const FrontendPath& path,
                    std::function<void(const FrontendPath&, const DirectoryEntryPtr)>&& fun)
{
    LOG_TRACE(path);

    // Avoid exploding the stack for deep directory hierarchies. Walking these is not
    // exactly a good idea in the first place as it'll result in too many arakoon lookups.
    // This in turn could be fixed by having harakoon return a cookie.
    std::stack<FrontendPath> stack;
    stack.push(path);

    while (not stack.empty())
    {
        const FrontendPath p(stack.top());
        stack.pop();

        LOG_TRACE(p);

        DirectoryEntryPtr dentry;

        try
        {
            dentry = find_throw(p);
        }
        catch (HierarchicalArakoon::DoesNotExistException&)
        {
            continue;
        }

        maybe_add_to_cache_(p, dentry);

        LOG_TRACE(path << ", dentry object id " << dentry->object_id());

        fun(p, dentry);

        if (dentry->type() == DirectoryEntry::Type::Directory)
        {
            std::list<std::string> l(list(p));
            for (const auto& e : l)
            {
                stack.push(FrontendPath(p / e));
            }
        }
    }
}

void
MetaDataStore::add(const FrontendPath& path,
                   DirectoryEntryPtr dentry)
{
    LOG_TRACE(path << ", dentry uuid " << dentry->object_id());

    const ArakoonPath p(path.string());
    const boost::optional<DirectoryEntry> oldval;

    try
    {
        harakoon_.test_and_set(p,
                               *dentry,
                               oldval);
    }
    catch (HierarchicalArakoon::PreconditionFailedException&)
    {
        // Someone else must've beaten us to adding an entry under 'path'.
#ifndef NDEBUG
        walk(FrontendPath("/"),
             [](const FrontendPath& fp,
                const DirectoryEntryPtr& /* dep */)
             {
                 LOG_DEBUG(fp);
             });
#endif
        throw FileExistsException("Metadata entry already exists",
                                  path.string().c_str(),
                                  EEXIST);
    }

    maybe_add_to_cache_(path, dentry);
}

void
MetaDataStore::add(const ObjectId& parent,
                   const std::string& name,
                   DirectoryEntryPtr dentry)
{
    LOG_TRACE(parent << ", dentry uuid " << dentry->object_id() << " name " << name);

    const yt::UUID p(parent.str());
    const boost::optional<DirectoryEntry> oldval;

    try
    {
        harakoon_.test_and_set(p,
                               name,
                               *dentry,
                               oldval);
    }
    catch (HierarchicalArakoon::PreconditionFailedException)
    {
        throw FileExistsException("Metadata entry already exists",
                                  name.c_str(),
                                  EEXIST);
    }

    FrontendPath path(find_path(parent));
    path /= name;

    maybe_add_to_cache_(path, dentry);
}

void
MetaDataStore::add_directories(const FrontendPath& path)
{
    LOG_TRACE(path);

    FrontendPath p;

    for (const auto& s : path)
    {
        p = FrontendPath(p / s);
        DirectoryEntryPtr dentry(find(p));

        while (true)
        {
            if (dentry == nullptr)
            {
                DirectoryEntryPtr
                    dentry(boost::make_shared<DirectoryEntry>(DirectoryEntry::Type::Directory,
                                                              alloc_inode(),
                                                              default_directory_permissions,
                                                              UserId(::getuid()),
                                                              GroupId(::getgid())));
                try
                {
                    struct timespec timebuf;
                    struct timeval tv;

                    TODO("X42: Better use fungi::IOException here? WIP");
                    int rc = clock_gettime(CLOCK_REALTIME, &timebuf);
                    if (rc != 0)
                    {
                        throw std::system_error(errno, std::system_category());
                    }

                    add(p, dentry);

                    tv.tv_sec = timebuf.tv_sec;
                    tv.tv_usec = timebuf.tv_nsec / 1000;
                    utimes(FrontendPath(p.parent_path()),
                           tv,
                           tv);
                    break;
                }
                catch (FileExistsException&)
                {
                    LOG_WARN(p << ": someone else created an eponymous entry at the same time");
                }
            }
            else
            {
                if (dentry->type() != DirectoryEntry::Type::Directory)
                {
                    LOG_ERROR(p << " already exists but is not a directory");
                    throw fungi::IOException("Entry is not a directory",
                                             p.string().c_str(),
                                             ENOTDIR);
                }

                break;
            }
        }
    }
}

void
MetaDataStore::unlink(const FrontendPath& p)
{
    LOG_TRACE(p);

    harakoon_.erase(HARAPATH(p));
    drop_from_cache(p);
}

void
MetaDataStore::unlink(const ObjectId& id,
                      const std::string& name)
{
    LOG_TRACE(id);

    FrontendPath path(find_path(id));
    path /= name;

    harakoon_.erase(yt::UUID(id.str()), name);
    drop_from_cache(path);
}

DirectoryEntryPtr
MetaDataStore::rename(const FrontendPath& from,
                      const FrontendPath& to)
{
    LOG_TRACE(from << " -> " << to);

    boost::optional<DirectoryEntryPtr>
        maybe_overwritten(harakoon_.rename<DirectoryEntry>(HARAPATH(from),
                                                           HARAPATH(to)));
    drop_from_cache(from);

    DirectoryEntryPtr dentry;

    if (maybe_overwritten)
    {
        drop_from_cache(to);
        dentry = *maybe_overwritten;
    }

    return dentry;
}

DirectoryEntryPtr
MetaDataStore::rename(const ObjectId& from_parent,
                      const std::string& from,
                      const ObjectId& to_parent,
                      const std::string& to)
{
    LOG_TRACE(from << " -> " << to);

    FrontendPath from_path(find_path(from_parent));
    from_path /= from;

    FrontendPath to_path(find_path(to_parent));
    to_path /= to;

    boost::optional<DirectoryEntryPtr>
        maybe_overwritten(harakoon_.rename<DirectoryEntry>(yt::UUID(from_parent.str()),
                                                           from,
                                                           yt::UUID(to_parent.str()),
                                                           to));
    drop_from_cache(from_path);

    DirectoryEntryPtr dentry;

    if (maybe_overwritten)
    {
        drop_from_cache(to_path);
        dentry = *maybe_overwritten;
    }

    return dentry;
}

}
