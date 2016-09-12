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

#ifndef LOCAL_CONNECTION_H_
#define LOCAL_CONNECTION_H_

#include "BackendConnectionInterface.h"
#include "BackendException.h"
#include "LocalConfig.h"

#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/thread/mutex.hpp>

#include <youtils/IOException.h>
#include <youtils/Logging.h>
#include <youtils/LRUCache.h>
#include <youtils/Weed.h>


// namespace youtils
// {
// template<typename A,
//          typename B,
//          template<typename...> class Zet = boost::bimaps::unordered_set_of>
// class LRUCache;
// }


namespace backend
{

VD_BOOLEAN_ENUM(EnablePartialRead);
VD_BOOLEAN_ENUM(SyncObjectAfterWrite)

namespace local
{

class Connection
    : public BackendConnectionInterface
{
public:
    typedef LocalConfig config_type;

    explicit Connection(const config_type& cfg);

    Connection(const boost::filesystem::path& path,
               const timespec& = {0, 0},
               const EnablePartialRead = EnablePartialRead::T,
               const SyncObjectAfterWrite = SyncObjectAfterWrite::T);

    virtual ~Connection();

    Connection(const Connection&) = delete;

    Connection&
    operator=(const Connection&) = delete;

    virtual bool
    healthy() const override final
    {
        return true;
    }

    virtual void
    listObjects_(const Namespace& nspace,
                 std::list<std::string>& objects) override final;

    virtual bool
    namespaceExists_(const Namespace& nspace) override final;

    virtual void
    createNamespace_(const Namespace& nspace,
                     const NamespaceMustNotExist = NamespaceMustNotExist::T) override final;

    virtual void
    deleteNamespace_(const Namespace& nspace) override final;

    virtual void
    listNamespaces_(std::list<std::string>& objects) override final;

    virtual void
    read_(const Namespace& nspace,
          const boost::filesystem::path& destination,
          const std::string& mame,
          InsistOnLatestVersion) override final;

    virtual std::unique_ptr<youtils::UniqueObjectTag>
    get_tag_(const Namespace&,
             const std::string&) override final;

    virtual void
    write_(const Namespace&,
           const boost::filesystem::path&,
           const std::string&,
           const OverwriteObject,
           const youtils::CheckSum* = nullptr,
           const boost::shared_ptr<Condition>& = nullptr) override final;

    virtual std::unique_ptr<youtils::UniqueObjectTag>
    write_tag_(const Namespace&,
               const boost::filesystem::path&,
               const std::string&,
               const youtils::UniqueObjectTag*,
               const OverwriteObject) override final;

    virtual std::unique_ptr<youtils::UniqueObjectTag>
    read_tag_(const Namespace&,
              const boost::filesystem::path&,
              const std::string&) override final;

    virtual bool
    objectExists_(const Namespace& nspace,
                  const std::string& name) override final;

    virtual void
    remove_(const Namespace&,
            const std::string&,
            const ObjectMayNotExist,
            const boost::shared_ptr<Condition>& = nullptr) override final;

    virtual uint64_t
    getSize_(const Namespace& nspace,
             const std::string& name) override final;

    virtual youtils::CheckSum
    getCheckSum_(const Namespace& nspace,
                 const std::string& name) override final;

    const boost::filesystem::path&
    path()
    {
        return path_;
    }

    virtual bool
    partial_read_(const Namespace& ns,
                  const PartialReads& partial_reads,
                  InsistOnLatestVersion) override final;

    using KeyType = boost::filesystem::path;
    using ValueType = youtils::FileDescriptor;

    using LRUCacheType = youtils::LRUCache<KeyType,
                                           ValueType>;

    const static size_t lru_cache_size = 32;

private:
    static void
    EvictFromCache(const KeyType&,
                   const ValueType&)
    {}

    static
    LRUCacheType&
    lruCache();

protected:
    DECLARE_LOGGER("LocalConnection");

    boost::filesystem::path path_;
    boost::interprocess::named_mutex lock_;
    struct timespec timespec_;
    const EnablePartialRead enable_partial_read_;
    const SyncObjectAfterWrite sync_object_after_write_;

    boost::filesystem::path
    nspacePath_(const Namespace& nspace) const;

    boost::filesystem::path
    checkedNamespacePath_(const Namespace& nspace) const;

    boost::filesystem::path
    objectPath_(const Namespace& nspace,
                const std::string& objname) const;

    boost::filesystem::path
    checkedObjectPath_(const Namespace& nspace,
                       const std::string& objname) const;

    void
    verify_tag_(const Namespace&,
                const std::string*,
                const youtils::UniqueObjectTag*) const;

    void
    copy_(const Namespace&,
          const boost::filesystem::path&,
          const std::string& obj_name,
          const OverwriteObject,
          const std::string* tag_name,
          const youtils::UniqueObjectTag* prev_tag);
};

}

}

#endif // !LOCAL_CONNECTION_H_

// Local Variables: **
// mode: c++ **
// End: **
