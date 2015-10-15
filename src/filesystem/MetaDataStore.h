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

#ifndef VFS_META_DATA_STORE_H_
#define VFS_META_DATA_STORE_H_

#include "ClusterId.h"
#include "DirectoryEntry.h"
#include "FrontendPath.h"
#include "HierarchicalArakoon.h"
#include "InodeAllocator.h"

#include <boost/thread/shared_mutex.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/make_shared.hpp>

#include <youtils/BooleanEnum.h>
#include <youtils/Logging.h>

namespace volumedriverfstest
{
class FileSystemTestBase;
}

namespace volumedriverfs
{

BOOLEAN_ENUM(UseCache);

template<typename T>
struct MetaDataStoreKeyTraits;

// TODO: this has no business being here - it's an implementation detail.
template<>
struct MetaDataStoreKeyTraits<FrontendPath>
{
    static const ArakoonPath
    make_key(const FrontendPath& p)
    {
        return static_cast<const ArakoonPath>(p);
    }

    static boost::optional<DirectoryEntry>
    make_old_val(const DirectoryEntry& p)
    {
        return boost::optional<DirectoryEntry>(p);
    }
};

template<>
struct MetaDataStoreKeyTraits<ObjectId>
{
    static const youtils::UUID
    make_key(const ObjectId& id)
    {
        return youtils::UUID(id.str());
    }

    static const DirectoryEntry&
    make_old_val(const DirectoryEntry& p)
    {
        return p;
    }
};

template<>
struct
HierarchicalArakoonUUIDAllocator<DirectoryEntry>
{
    static youtils::UUID alloc_uuid(const DirectoryEntry dentry)
    {
        return youtils::UUID(dentry.object_id().str());
    };
};

class MetaDataStore
{
    friend class volumedriverfstest::FileSystemTestBase;

public:
    MetaDataStore(std::shared_ptr<youtils::LockedArakoon> larakoon,
                  const ClusterId& cid,
                  UseCache use_cache);

    ~MetaDataStore() = default;

    MetaDataStore(const MetaDataStore&) = delete;

    MetaDataStore&
    operator=(const MetaDataStore&) = delete;

    static void
    destroy(std::shared_ptr<youtils::LockedArakoon> larakoon,
            const ClusterId& cid);

    void
    add(const FrontendPath& p, DirectoryEntryPtr dentry);

    void
    add(const ObjectId& parent,
        const std::string& name,
        DirectoryEntryPtr dentry);

    void
    add_directories(const FrontendPath& p);

    template<typename T,
             typename Traits = MetaDataStoreKeyTraits<T>>
    DirectoryEntryPtr
    find_throw(const T& id)
    {
        DirectoryEntryPtr dentry(find_in_cache_(id));
        if (dentry == nullptr)
        {
            dentry =
                DirectoryEntryPtr(harakoon_.get<DirectoryEntry>(Traits::make_key(id)));
            maybe_add_to_cache_(id, dentry);
        }

        ASSERT(dentry != nullptr);

        return dentry;
    }

    template<typename T,
             typename Traits = MetaDataStoreKeyTraits<T>>
    DirectoryEntryPtr
    find(const T& id)
    {
        try
        {
            return find_throw<T, Traits>(id);
        }
        catch (HierarchicalArakoon::DoesNotExistException&)
        {
            return nullptr;
        }
    }

    void
    walk(const FrontendPath& p,
         std::function<void(const FrontendPath&, const DirectoryEntryPtr)>&& fun);

    template<typename T,
             typename Traits = MetaDataStoreKeyTraits<T>>
    void
    chown(const T& id,
          boost::optional<UserId>& uid,
          boost::optional<GroupId>& gid)
    {
        LOG_TRACE(id << ", uid " << uid << ", gid " << gid);

        if (uid or gid)
        {
            retry_test_and_set_<T, Traits>(id,
                                           [&](DirectoryEntry& dentry)
                                           {
                                               if (uid)
                                               {
                                                   dentry.user_id(*uid);
                                               }

                                               if (gid)
                                               {
                                                   dentry.group_id(*gid);
                                               }
                                           });
        }
    }

    template<typename T,
             typename Traits = MetaDataStoreKeyTraits<T>>
    void
    chmod(const T& id,
          Permissions pms)
    {
        LOG_TRACE(id << ", permissions " << pms);

        retry_test_and_set_<T, Traits>(id,
                                       [&](DirectoryEntry& dentry)
             {
                 dentry.permissions(pms);
             });
    }

    template<typename T,
             typename Traits = MetaDataStoreKeyTraits<T>>
    void
    utimes(const T& id,
           const timeval& atime,
           const timeval& mtime)
    {
            LOG_TRACE(id);

            retry_test_and_set_<T, Traits>(id,
                                           [&](DirectoryEntry& dentry)
             {
                 dentry.atime(atime);
                 dentry.mtime(mtime);
             });
    }

    void
    unlink(const FrontendPath& p);

    void
    unlink(const ObjectId& id,
           const std::string& name);

    // returns the old entry behind "to" (if any)
    DirectoryEntryPtr
    rename(const FrontendPath& from,
           const FrontendPath& to);

    DirectoryEntryPtr
    rename(const ObjectId& from_parent,
           const std::string& from,
           const ObjectId& to_parent,
           const std::string& to);

    template<typename T,
             typename Traits = MetaDataStoreKeyTraits<T>>
    std::list<std::string>
    list(const T& id)
    {
        LOG_TRACE(id);
        return harakoon_.list(Traits::make_key(id));
    }

    FrontendPath
    find_path(const ObjectId& id);

    Inode
    alloc_inode()
    {
        return inode_alloc_();
    }

    // Yuck. This one exposes the presence of the otherwise nicely transparent cache.
    // Raison d'etre is that the cache on node N could contain entries that were removed
    // on node M.
    // Move the cache to consumers instead? Or make the MetaDataStore smarter / cluster
    // aware which would also allow it to cache regular file / directory entries?
    void
    drop_from_cache(const FrontendPath& p);

    void
    drop_from_cache(const ObjectId& id);

    UseCache
    use_cache() const;

private:
    DECLARE_LOGGER("VFSMetaDataStore");

    HierarchicalArakoon harakoon_;
    InodeAllocator inode_alloc_;

    UseCache use_cache_;

    mutable boost::shared_mutex cache_lock_;

    // Only contains entries for volumes at the moment.
    typedef boost::bimap<boost::bimaps::set_of<FrontendPath>,
                         boost::bimaps::set_of<ObjectId>,
                         boost::bimaps::with_info<DirectoryEntryPtr> > bimap_cache_t;

    typedef bimap_cache_t::value_type cache_entry_t;

    bimap_cache_t bimap_cache_;

    void
    maybe_initialise_();

    void
    maybe_add_to_cache_(const FrontendPath& p, DirectoryEntryPtr& dentry);

    void
    maybe_add_to_cache_(const ObjectId& id, DirectoryEntryPtr& dentry);

    DirectoryEntryPtr
    find_in_cache_(const FrontendPath& p);

    DirectoryEntryPtr
    find_in_cache_(const ObjectId& id);

    template<typename T,
             typename Traits = MetaDataStoreKeyTraits<T>>
    void
    retry_test_and_set_(const T& id,
                        std::function<void(DirectoryEntry& dentry)> fun)
    {
        LOG_TRACE(id);

        while (true)
        {
            DirectoryEntryPtr oldval(find_throw(id));
            DirectoryEntryPtr newval(boost::make_shared<DirectoryEntry>(*oldval));

            try
            {
                fun(*newval);
                harakoon_.test_and_set(Traits::make_key(id),
                                       *newval,
                                       Traits::make_old_val(*oldval));
                maybe_add_to_cache_(id,
                                    newval);
                break;
            }
            catch (HierarchicalArakoon::PreconditionFailedException&)
            {
                LOG_WARN(id << ": conflicting update detected - retrying");
                drop_from_cache(id);
            }
        }
    }
};

}

#endif // !VFS_META_DATA_STORE_H_
