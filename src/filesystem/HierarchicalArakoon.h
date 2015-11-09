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

#ifndef VFS_HIERARCHICAL_ARAKOON_H_
#define VFS_HIERARCHICAL_ARAKOON_H_

#include <map>
#include <memory>
#include <type_traits>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/map.hpp>

#include <youtils/IOException.h>
#include <youtils/LockedArakoon.h>
#include <youtils/Logging.h>
#include <youtils/StrongTypedPath.h>
#include <youtils/UUID.h>

// Push this thing to youtils?

// rules for ArakoonPaths:
// * must not be empty
// * must be absolute
// * no trailing slashes, no "." or ".." path components
STRONG_TYPED_PATH(volumedriverfs, ArakoonPath);

STRONG_TYPED_STRING(volumedriverfs, ArakoonEntryId);

namespace volumedriverfstest
{
class HierarchicalArakoonTest;
}

// Ideas:
// * With the exception of the root_ entry, keys are UUIDs.
// * The root_ entry (named <prefix>/root) points to the actual "/" entry.
//   This adds an extra indirection (TODO: avoid the extra trip to arakoon by introducing
//   caching) but avoids special casing for "/".
// * Values consist of a boost::serialization archive and the actual payload. The archive
//   contains an Entry which has a map of child names -> child UUIDs
// * The payload is by default also using a boost::serialization archive and serialized by
//   pointer - this way consumers are not forced to provide a default constructor.
namespace volumedriverfs
{

// XXX: try to unify with arakoon::DataBufferTraits?
template<typename T>
struct HierarchicalArakoonValueTraits
{
    static_assert(not std::is_pointer<T>::value,
                  "write/fix traits for pointers");

    static_assert(not std::is_lvalue_reference<T>::value,
                  "write/fix traits for lvalue references");

    static_assert(not std::is_rvalue_reference<T>::value,
                  "write/fix traits for rvalue references");

    typedef boost::archive::text_iarchive IArchive;
    typedef boost::archive::text_oarchive OArchive;

    static void
    serialize(std::ostream& os, const T& t)
    {
        OArchive oa(os);
        const T* ptr = &t;
        oa << ptr;
    }

    typedef boost::shared_ptr<T> DeserializedType;

    static DeserializedType
    deserialize(std::istream& is)
    {
        IArchive ia(is);
        T* ptr = nullptr;

        try
        {
            ia >> ptr;
        }
        catch (...)
        {
            delete ptr;
            throw;
        }

        return DeserializedType(ptr);
    }
};

template<typename T>
struct HierarchicalArakoonUUIDAllocator
{
    static youtils::UUID alloc_uuid(const T& /* t */)
    {
        return youtils::UUID();
    };
};

class HierarchicalArakoon
{
    friend class volumedriverfstest::HierarchicalArakoonTest;

    using EntryMap = std::map<std::string, ArakoonEntryId>;
    using MaybeArakoonEntryId = boost::optional<ArakoonEntryId>;

    struct Entry
        : public EntryMap
    {
        Entry(const ArakoonEntryId& i,
              const MaybeArakoonEntryId& pi)
            : id(i)
            , parent_id(pi)
        {}

        Entry() = default;

        ~Entry() = default;

        Entry(const Entry&) = default;

        Entry&
        operator=(const Entry&) = default;

        template<typename Archive>
        void serialize(Archive& ar, const unsigned version)
        {
            CHECK_VERSION(version, 2);
            ar & boost::serialization::make_nvp("children",
                                                boost::serialization::base_object<EntryMap>(*this));

            ar & boost::serialization::make_nvp("id",
                                                const_cast<ArakoonEntryId&>(id));

            ar & boost::serialization::make_nvp("parent_id",
                                                parent_id);
        }

        const ArakoonEntryId id;
        MaybeArakoonEntryId parent_id;
    };

    typedef boost::archive::text_iarchive IArchive;
    typedef boost::archive::text_oarchive OArchive;

    friend class boost::serialization::access;

public:
    MAKE_EXCEPTION(DoesNotExistException, fungi::IOException);
    MAKE_EXCEPTION(NotEmptyException, fungi::IOException);
    MAKE_EXCEPTION(InvalidPathException, fungi::IOException);
    MAKE_EXCEPTION(PreconditionFailedException, fungi::IOException);

    HierarchicalArakoon(std::shared_ptr<youtils::LockedArakoon> arakoon,
                        const std::string& prefix);

    ~HierarchicalArakoon() = default;

    HierarchicalArakoon(const HierarchicalArakoon&) = default;

    HierarchicalArakoon&
    operator=(const HierarchicalArakoon&) = default;

    bool
    initialized();

    static void
    destroy(std::shared_ptr<youtils::LockedArakoon> arakoon,
            const std::string& prefix);

    template<typename T,
             typename Traits = HierarchicalArakoonValueTraits<T>,
             typename UUIDAllocator = HierarchicalArakoonUUIDAllocator<T> >
    void
    initialize(const T& value)
    {
        LOG_INFO("Initializing cluster ID " << arakoon_->cluster_id() <<
                 ", prefix " << prefix_);

        auto fun([&](arakoon::sequence& seq)
                 {
                     const std::string rkey(make_key_(root_));

                     Entry root(root_,
                                boost::none);
                     const Entry slash(ArakoonEntryId(UUIDAllocator::alloc_uuid(value).str()), root_);

                     root.emplace("/", slash.id);

                     const std::string skey(make_key_(slash.id));

                     LOG_INFO("Adding " << root_ << " entry (" << rkey <<
                              ") and entry for \"/\" (" << skey << ")");

                     std::stringstream ss;
                     {
                         OArchive oa(ss);
                         oa << root;
                     }

                     seq.add_assert(rkey, arakoon::None());
                     seq.add_assert(skey, arakoon::None());
                     seq.add_set(rkey, ss.str());
                     seq.add_set(skey, serialize_entry_<T, Traits>(slash, value));
                 });

        arakoon_->run_sequence("initialize hierarchical arakoon",
                               fun,
                               youtils::RetryOnArakoonAssert::F);
    }

    template<typename T,
             typename Traits = HierarchicalArakoonValueTraits<T>,
             typename UUIDAllocator = HierarchicalArakoonUUIDAllocator<T> >
    void
    set(const ArakoonPath& path,
        const T& value)
    {
        LOG_TRACE(path);

        boost::optional<std::string> pkey;

        arakoon_->run_sequence("set entry",
                               [&](arakoon::sequence& seq)
                               {
                                   prepare_test_and_set_sequence_<T,
                                                                  Traits,
                                                                  UUIDAllocator>(path,
                                                                                 pkey,
                                                                                 false,
                                                                                 value,
                                                                                 boost::none,
                                                                                 seq);
                               },
                               youtils::RetryOnArakoonAssert::T);
    }

    template<typename T,
             typename Traits = HierarchicalArakoonValueTraits<T>,
             typename UUIDAllocator = HierarchicalArakoonUUIDAllocator<T> >
    void
    set(const youtils::UUID& parent_id,
        const std::string& name,
        const T& value)
    {
        arakoon_->run_sequence("set entry",
                                [&](arakoon::sequence& seq)
                                {
                                    prepare_test_and_set_sequence_<T,
                                                                   Traits,
                                                                   UUIDAllocator>(parent_id,
                                                                                  name,
                                                                                  false,
                                                                                  value,
                                                                                  boost::none,
                                                                                  seq);
                                },
                                youtils::RetryOnArakoonAssert::T);
    }

    template<typename T,
             typename Traits = HierarchicalArakoonValueTraits<T>,
             typename UUIDAllocator = HierarchicalArakoonUUIDAllocator<T> >
    void
    test_and_set(const ArakoonPath& path,
                 const T& newval,
                 const boost::optional<T>& oldval)
    {
        LOG_TRACE(path);

        boost::optional<std::string> pkey;

        arakoon_->run_sequence("set entry",
                               [&](arakoon::sequence& seq)
                               {
                                   prepare_test_and_set_sequence_<T,
                                                                  Traits,
                                                                  UUIDAllocator>(path,
                                                                                 pkey,
                                                                                 true,
                                                                                 newval,
                                                                                 oldval,
                                                                                 seq);
                               },
                               youtils::RetryOnArakoonAssert::T);
    }

    template<typename T,
             typename Traits = HierarchicalArakoonValueTraits<T>,
             typename UUIDAllocator = HierarchicalArakoonUUIDAllocator<T> >
    void
    test_and_set(const youtils::UUID& parent_id,
                 const std::string& name,
                 const T& newval,
                 const boost::optional<T>& oldval)
    {

        arakoon_->run_sequence("set entry",
                                [&](arakoon::sequence& seq)
                                {
                                    prepare_test_and_set_sequence_<T,
                                                                   Traits,
                                                                   UUIDAllocator>(parent_id,
                                                                                  name,
                                                                                  true,
                                                                                  newval,
                                                                                  oldval,
                                                                                  seq);
                                },
                                youtils::RetryOnArakoonAssert::T);
    }

    template<typename T,
             typename Traits = HierarchicalArakoonValueTraits<T>,
             typename UUIDAllocator = HierarchicalArakoonUUIDAllocator<T> >
    void
    test_and_set(const youtils::UUID& id,
                 const T& newval,
                 const T& oldval)
    {
        arakoon_->run_sequence("set entry",
                                [&](arakoon::sequence& seq)
                                {
                                    prepare_test_and_set_sequence_<T,
                                                                   Traits,
                                                                   UUIDAllocator>(id,
                                                                                  newval,
                                                                                  oldval,
                                                                                  seq);
                                },
                                youtils::RetryOnArakoonAssert::T);
    }

    template<typename T,
             typename Traits = HierarchicalArakoonValueTraits<T> >
    typename Traits::DeserializedType
    get(const ArakoonPath& path)
    {
        LOG_TRACE(path);

        auto fun([&path, this](std::istream& is) -> typename Traits::DeserializedType
                 {
                     deserialize_entry_(is);
                     return Traits::deserialize(is);
                 });

        return find_(path).as_istream<typename Traits::DeserializedType>(fun);
    }

    template<typename T,
             typename Traits = HierarchicalArakoonValueTraits<T> >
    typename Traits::DeserializedType
    get(const youtils::UUID& id)
    {
        LOG_TRACE(id);

        auto fun([&id, this](std::istream& is) -> typename Traits::DeserializedType
                 {
                     deserialize_entry_(is);
                     return Traits::deserialize(is);
                 });
        arakoon::buffer buf;
        try
        {
            buf = arakoon_->get(make_key_(id.str()));
        }
        catch (arakoon::error_not_found&)
        {
            LOG_DEBUG(id << " does not exist");
            throw DoesNotExistException("UUID does not exist",
                                        id.str().c_str(),
                                        ENOENT);
        }
        return buf.as_istream<typename Traits::DeserializedType>(fun);
    }

    void
    erase(const ArakoonPath& path);

    void
    erase(const youtils::UUID& parent_id, const std::string& name);

    std::list<std::string>
    list(const ArakoonPath& parent);

    std::list<std::string>
    list(const youtils::UUID& id);

    template<typename T,
             typename Traits = HierarchicalArakoonValueTraits<T> >
    boost::optional<typename Traits::DeserializedType>
    rename(const ArakoonPath& from,
           const ArakoonPath& to)
    {
        LOG_TRACE("Attempting to rename " << from << " -> " << to);

        boost::optional<std::string> pfrom;
        boost::optional<std::string> pto;
        boost::optional<typename Traits::DeserializedType> ret;

        // XXX: Hmm. We might not want to take this shortcut to check invalid / inexistent
        // paths?
        if (from == to)
        {
            LOG_TRACE("nothing to do");
        }
        else if (is_prefix_(from, to))
        {
            LOG_ERROR(from << " is a prefix of " << to << " - renaming not permitted");
            throw InvalidPathException("source path is prefix of destination path",
                                       to.string().c_str(),
                                       EPERM);
        }
        else
        {
            arakoon_->run_sequence("rename entry",
                                   [&](arakoon::sequence& seq)
                                   {
                                       ret = prepare_rename_sequence_<T, Traits>(from,
                                                                                 to,
                                                                                 pfrom,
                                                                                 pto,
                                                                                 seq);
                                   },
                                   youtils::RetryOnArakoonAssert::T);
        }

        return ret;
    }

    template<typename T,
             typename Traits = HierarchicalArakoonValueTraits<T> >
    boost::optional<typename Traits::DeserializedType>
    rename(const youtils::UUID& from_parent_id, const std::string& from_name,
           const youtils::UUID& to_parent_id, const std::string& to_name)
    {
        boost::optional<typename Traits::DeserializedType> ret;

        if (from_parent_id == to_parent_id and from_name == to_name)
        {
            LOG_TRACE("nothing to do");
        }
        else
        {
            arakoon_->run_sequence("rename entry",
                                   [&](arakoon::sequence& seq)
                                   {
                                    ret = prepare_rename_sequence_<T, Traits>(from_parent_id,
                                                                              from_name,
                                                                              to_parent_id,
                                                                              to_name,
                                                                              seq);
                                   },
                                   youtils::RetryOnArakoonAssert::T);
        }

        return ret;
    }

    ArakoonEntryId
    get_id_by_path(const ArakoonPath& path)
    {
        LOG_TRACE(path);

        arakoon::buffer buf;

        try
        {
            buf = find_(path);
        }
        catch (arakoon::error_not_found&)
        {
            LOG_DEBUG(path << " does not exist");
            throw DoesNotExistException("Path does not exist",
                                         path.str().c_str(),
                                         ENOENT);
        }

        return buf.as_istream<ArakoonEntryId>([&](std::istream& is) -> ArakoonEntryId
               {
                   return deserialize_entry_(is).id;
               });
    }

    // better: boost::optional<ArakoonPath>?
    ArakoonPath
    get_path_by_id(const youtils::UUID& id)
    {
        LOG_TRACE("id: " << id);

        boost::filesystem::path path;
        std::string pkey(make_key_(id.str()));
        std::string mkey(id.str());
        bool stop = false;

        while (not stop)
        {
            arakoon::buffer pbuf;

            try
            {
                pbuf = arakoon_->get(pkey);
            }
            catch(arakoon::error_not_found&)
            {
                LOG_DEBUG(pkey << " does not exist");
                throw DoesNotExistException("key does not exist",
                                            pkey.c_str(),
                                            ENOENT);
            }

            pbuf.as_istream<void>([&](std::istream& is)
                                  {
                                      const Entry pentry(deserialize_entry_(is));
                                      for (const auto& x : pentry)
                                      {
                                          if (x.second == mkey)
                                          {
                                              path =
                                                  boost::filesystem::path(x.first) /
                                                  path;
                                              break;
                                          }
                                      }
                                      if (pentry.parent_id == boost::none)
                                      {
                                          stop = true;
                                      }
                                      else
                                      {
                                          VERIFY(not path.is_absolute());
                                          pkey =
                                              make_key_(pentry.parent_id->str());
                                          mkey = pentry.id;
                                      }
                                  });
        }

        VERIFY(path.is_absolute());
        return ArakoonPath(path);
    }

private:
    DECLARE_LOGGER("HierarchicalArakoon");

    static const ArakoonEntryId root_;

    std::shared_ptr<youtils::LockedArakoon> arakoon_;
    const std::string prefix_;

    static void
    validate_path_(const boost::filesystem::path& path);

    bool
    is_prefix_(const boost::filesystem::path& pfx,
               const boost::filesystem::path& path);

    std::string
    make_key_(const std::string& k) const;

    Entry
    deserialize_entry_(std::istream& is);

    Entry
    deserialize_entry_(const arakoon::buffer& buf);

    arakoon::buffer
    do_find_(const boost::filesystem::path& path);

    arakoon::buffer
    find_(const boost::filesystem::path& path);

    arakoon::buffer
    find_parent_(const boost::filesystem::path& path);

    void
    do_prepare_erase_sequence_(const arakoon::buffer& pbuf,
                               Entry& pentry,
                               std::istream& is,
                               const std::string& name,
                               boost::optional<std::string>& pkey,
                               arakoon::sequence& seq);

    void
    prepare_erase_sequence_(const boost::filesystem::path& path,
                            boost::optional<std::string>& pkey,
                            arakoon::sequence& seq);

    void
    prepare_erase_sequence_(const youtils::UUID& parent_id,
                            const std::string& name,
                            arakoon::sequence& seq);

    static std::string
    update_serialized_entry_(const Entry& e, std::istream& is);

    template<typename T,
             typename Traits>
    static std::string
    serialize_entry_(const Entry& e, const T& value)
    {
        std::stringstream ss;

        {
            OArchive oa(ss);
            oa << e;
        }

        Traits::serialize(ss, value);
        return ss.str();
    }

    template<typename R>
    R
    with_parent_(const boost::filesystem::path& path,
                 boost::optional<std::string>& pkey,
                 std::function<R(const arakoon::buffer&,
                                 Entry& parent,
                                 std::istream&)>&& fun)
    {
        LOG_TRACE(path << ", pkey " << pkey);

        arakoon::buffer pbuf;
        if (not pkey)
        {
            pbuf = find_parent_(path);
        }
        else
        {
            pbuf = arakoon_->get(*pkey);
        }

        return pbuf.as_istream<R>([&](std::istream& is) -> R
            {
                Entry pentry(deserialize_entry_(is));
                if (not pkey)
                {
                    pkey = make_key_(pentry.id);
                }

                return fun(pbuf, pentry, is);
            });
    }

    template<typename R>
    R
    with_parent_(const youtils::UUID& parent_id,
                 std::function<R(const arakoon::buffer&,
                                 Entry& parent,
                                 std::istream&)>&& fun)
    {
        LOG_TRACE("parent id " << parent_id)

        arakoon::buffer pbuf;

        pbuf = arakoon_->get(make_key_(parent_id.str()));

        return pbuf.as_istream<R>([&](std::istream& is) -> R
            {
                Entry pentry(deserialize_entry_(is));
                return fun(pbuf, pentry, is);
            });
    }

    template<typename T, typename Traits, typename UUIDAllocator>
    void
    do_prepare_test_and_set_sequence_(const arakoon::buffer& pbuf,
                                      Entry& pentry,
                                      std::istream& is,
                                      const std::string& name,
                                      boost::optional<std::string>& pkey,
                                      bool test_and_set,
                                      const T& newval,
                                      const boost::optional<T>& oldval,
                                      arakoon::sequence& seq)
    {
        seq.add_assert(*pkey, pbuf);

        auto it = pentry.find(name);
        if (it == pentry.end())
        {
            if (test_and_set and oldval)
            {
                LOG_DEBUG(name <<
                          ": precondition failed: expected not to find a value");
                throw PreconditionFailedException("Precondition failed: value already present",
                                                  name.c_str());
            }

            Entry entry(ArakoonEntryId(UUIDAllocator::alloc_uuid(newval).str()),
                        ArakoonEntryId(pentry.id));
            pentry.emplace(name, entry.id);

            LOG_TRACE(name << " not present yet - adding new entry " << entry.id);

            seq.add_assert(*pkey, pbuf);
            seq.add_set(*pkey, update_serialized_entry_(pentry, is));
            seq.add_set(make_key_(entry.id), serialize_entry_<T, Traits>(entry, newval));
        }
        else
        {
            LOG_TRACE(name << " already present, id " << it->second.str());

            if (test_and_set and not oldval)
            {
                LOG_DEBUG(name <<
                          ": precondition failed: expected to find a value");
                throw PreconditionFailedException("Precondition failed: value not present",
                                                  name.c_str());
            }

            const std::string key(make_key_(it->second));
            const arakoon::buffer cbuf(arakoon_->get(key));

            cbuf.as_istream<void>([&](std::istream& is)
            {
                Entry entry(deserialize_entry_(is));
                auto ptr(Traits::deserialize(is));

                VERIFY(ptr);
                if (test_and_set and *oldval != *ptr)
                {
                    LOG_DEBUG(name <<
                              ": precondition failed: old value does not match expectations");
                    throw PreconditionFailedException("Precondition failed: old value does not match expectations",
                                                      name.c_str());
                }

                seq.add_assert(key, cbuf);
                seq.add_set(key, serialize_entry_<T, Traits>(entry, newval));
            });
        }
    }

    template<typename T, typename Traits, typename UUIDAllocator>
    void
    prepare_test_and_set_sequence_(const boost::filesystem::path& path,
                                   boost::optional<std::string>& pkey,
                                   bool test_and_set,
                                   const T& newval,
                                   const boost::optional<T>& oldval,
                                   arakoon::sequence& seq)
    {
        LOG_TRACE(path << ", test and set: " << test_and_set <<
                  " (parent: " << pkey << ")");

        with_parent_<void>(path,
                           pkey,
                           [&](const arakoon::buffer& pbuf,
                               Entry& pentry,
                               std::istream& is)
                           {
                                do_prepare_test_and_set_sequence_<T,
                                                                  Traits,
                                                                  UUIDAllocator>(pbuf,
                                                                                 pentry,
                                                                                 is,
                                                                                 path.filename().string(),
                                                                                 pkey,
                                                                                 test_and_set,
                                                                                 newval,
                                                                                 oldval,
                                                                                 seq);
                           });
    }

    template<typename T, typename Traits, typename UUIDAllocator>
    void
    prepare_test_and_set_sequence_(const youtils::UUID& parent_id,
                                   const std::string& name,
                                   bool test_and_set,
                                   const T& newval,
                                   const boost::optional<T>& oldval,
                                   arakoon::sequence& seq)
    {
        LOG_TRACE(parent_id << ", test and set: " << test_and_set <<
                  " (name: " << name << ")");
        with_parent_<void>(parent_id,
                [&](const arakoon::buffer& pbuf,
                    Entry& pentry,
                    std::istream& is)
                {
                    boost::optional<std::string> pkey(make_key_(parent_id.str()));

                    do_prepare_test_and_set_sequence_<T,
                                                      Traits,
                                                      UUIDAllocator>(pbuf,
                                                                     pentry,
                                                                     is,
                                                                     name,
                                                                     pkey,
                                                                     test_and_set,
                                                                     newval,
                                                                     oldval,
                                                                     seq);
                });
    }

    template<typename T, typename Traits, typename UUIDAllocator>
    void
    prepare_test_and_set_sequence_(const youtils::UUID& id,
                                   const T& newval,
                                   const T& oldval,
                                   arakoon::sequence& seq)
    {
        try
        {
            const std::string key(make_key_(id.str()));
            const arakoon::buffer cbuf(arakoon_->get(key));

            cbuf.as_istream<void>([&](std::istream& is)
            {
                Entry entry(deserialize_entry_(is));
                auto ptr(Traits::deserialize(is));

                VERIFY(ptr);
                if (oldval != *ptr)
                {
                    LOG_DEBUG(id <<
                              ": precondition failed: old value does not match expectations");
                    throw PreconditionFailedException("Precondition failed: old value does not match expectations");
                }

                seq.add_assert(key, cbuf);
                seq.add_set(key,
                    serialize_entry_<T, Traits>(entry, newval));
            });
        }
        catch (arakoon::error_not_found&)
        {
            LOG_DEBUG(id << " does not exist");
            throw DoesNotExistException("UUID does not exist",
                                        id.str().c_str(),
                                        ENOENT);
        }
    }

    template<typename T,
             typename Traits>
    boost::optional<typename Traits::DeserializedType>
    do_prepare_rename_sequence_(const arakoon::buffer& buf_from,
                                const arakoon::buffer& buf_to,
                                const std::string& from,
                                const std::string& to,
                                boost::optional<std::string>& parent_from,
                                boost::optional<std::string>& parent_to,
                                arakoon::sequence& seq)
    {
        boost::optional<typename Traits::DeserializedType> ret;

        buf_from.as_istream<void>([&](std::istream& is_from)
        {
            buf_to.as_istream<void>([&](std::istream& is_to)
            {
                Entry entry_from(deserialize_entry_(is_from));
                if (not parent_from)
                {
                    parent_from = make_key_(entry_from.id);
                }

                auto it_from = entry_from.find(from);
                if (it_from == entry_from.end())
                {
                    LOG_ERROR(from << " does not exist");
                    throw DoesNotExistException("Source path/uuid does not exist",
                                                from.c_str(),
                                                ENOENT);
                }

                const ArakoonEntryId entry_id(it_from->second);

                if (parent_to)
                {
                     /* No need for the path based functions */
                    if (make_key_(it_from->second.str()) == *parent_to)
                    {
                        LOG_ERROR(from << " is a prefix of " << to << " - renaming not permitted");
                        throw InvalidPathException("source path is prefix of destination path",
                                                   to.c_str(),
                                                   EPERM);
                    }
                }

                Entry entry_to(deserialize_entry_(is_to));
                if (not parent_to)
                {
                    parent_to = make_key_(entry_to.id);
                }

                auto it_to = entry_to.find(to);
                if (it_to != entry_to.end())
                {
                    LOG_INFO(from << " -> " << to <<
                             ": destination path/uuid already exists (" <<
                             it_to->second << ") - overwriting it");

                    const std::string old_key(make_key_(it_to->second.str()));
                    arakoon::buffer garbage(arakoon_->get(old_key));
                    garbage.as_istream<void>([&](std::istream& is)
                                             {
                                                 deserialize_entry_(is);
                                                 ret = Traits::deserialize(is);
                                             });
                    seq.add_delete(old_key);
                }

                if (parent_from == parent_to)
                {
                    entry_from.erase(from);
                    entry_from[to] = entry_id;

                    seq.add_assert(*parent_from, buf_from);
                    seq.add_set(*parent_from, update_serialized_entry_(entry_from,
                                                                       is_from));
                }
                else
                {
                    /* Fetch and change parent UUID for entry_id */
                    arakoon::buffer buf_entry = arakoon_->get(make_key_(entry_id.str()));
                    buf_entry.as_istream<void>([&](std::istream& is_entry)
                    {
                        Entry entry_up(deserialize_entry_(is_entry));
                        entry_up.parent_id = ArakoonEntryId(entry_to.id);

                        entry_from.erase(from);
                        entry_to[to] = entry_id;

                        seq.add_assert(*parent_from, buf_from);
                        seq.add_assert(*parent_to, buf_to);
                        seq.add_assert(make_key_(entry_id.str()), buf_entry);

                        seq.add_set(*parent_from,
                                    update_serialized_entry_(entry_from, is_from));
                        seq.add_set(*parent_to,
                                    update_serialized_entry_(entry_to, is_to));
                        seq.add_set(make_key_(entry_id.str()),
                                    update_serialized_entry_(entry_up, is_entry));
                    });
                }
            });
        });
        return ret;
    }

    template<typename T,
             typename Traits>
    boost::optional<typename Traits::DeserializedType>
    prepare_rename_sequence_(const boost::filesystem::path& from,
                             const boost::filesystem::path& to,
                             boost::optional<std::string>& parent_from,
                             boost::optional<std::string>& parent_to,
                             arakoon::sequence& seq)
    {
        LOG_TRACE(from << " (parent: " << parent_from << ") -> " <<
                  to << " (parent: " << parent_to << ")");

        arakoon::buffer buf_to;
        arakoon::buffer buf_from;
        boost::optional<typename Traits::DeserializedType> ret;

        // TODO: be smarter if "from" and "to" have the same parent.
        if (not parent_from)
        {
            VERIFY(!parent_to);
            buf_from = find_parent_(from);
            buf_to = find_parent_(to);
        }
        else
        {
            VERIFY(parent_to);
            buf_from = arakoon_->get(*parent_from);
            buf_to = arakoon_->get(*parent_to);
        }

        ret = do_prepare_rename_sequence_<T, Traits>(buf_from,
                                                     buf_to,
                                                     from.filename().string(),
                                                     to.filename().string(),
                                                     parent_from,
                                                     parent_to,
                                                     seq);

        return ret;
    }

    template<typename T,
             typename Traits>
    boost::optional<typename Traits::DeserializedType>
    prepare_rename_sequence_(const youtils::UUID& from_parent_id,
                             const std::string& from_name,
                             const youtils::UUID& to_parent_id,
                             const std::string& to_name,
                             arakoon::sequence& seq)
    {
        LOG_TRACE(from_name << "(parent id: " << from_parent_id << ") -> " <<
                to_name << " (parent id: " << to_parent_id << ")");

        arakoon::buffer buf_to;
        arakoon::buffer buf_from;
        boost::optional<typename Traits::DeserializedType> ret;

        buf_from = arakoon_->get(make_key_(from_parent_id.str()));
        buf_to = arakoon_->get(make_key_(to_parent_id.str()));

        boost::optional<std::string> parent_from(make_key_(from_parent_id.str()));
        boost::optional<std::string> parent_to(make_key_(to_parent_id.str()));

        ret = do_prepare_rename_sequence_<T, Traits>(buf_from,
                                                     buf_to,
                                                     from_name,
                                                     to_name,
                                                     parent_from,
                                                     parent_to,
                                                     seq);

        return ret;
    }
};
}

BOOST_CLASS_VERSION(volumedriverfs::HierarchicalArakoon::Entry, 2);

#endif // !VFS_HIERARCHICAL_ARAKOON_H_
