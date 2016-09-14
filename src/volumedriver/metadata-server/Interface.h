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

#ifndef META_DATA_SERVER_INTERFACE_H_
#define META_DATA_SERVER_INTERFACE_H_

#include <atomic>
#include <functional>
#include <iosfwd>
#include <memory>
#include <stdexcept>
#include <vector>

#include <boost/optional.hpp>

#include <youtils/BooleanEnum.h>

#include <volumedriver/OwnerTag.h>
#include <volumedriver/ScrubId.h>
#include <volumedriver/Types.h>

VD_BOOLEAN_ENUM(Barrier);

namespace metadata_server
{

// duplicates DataBufferTraits from Arakoon.
template <typename T>
struct DataBufferTraits
{};

template<>
struct DataBufferTraits<std::string>
{
    static size_t
    size(const std::string& in)
    {
        return in.size();
    }

    static const void*
    data(const std::string& in)
    {
        return in.data();
    }
};

template<>
struct DataBufferTraits<uint64_t>
{
    static size_t
    size(const uint64_t&)
    {
        return sizeof(uint64_t);
    }

    static const void*
    data(const uint64_t& key)
    {
        return &key;
    }
};

struct None
{};

template<>
struct DataBufferTraits<None>
{
    static size_t
    size(const None&)
    {
        return 0;
    }

    static const void*
    data(const None&)
    {
        return nullptr;
    }
};

template<bool may_be_null>
struct Item
{
    template<typename T,
             typename Traits = DataBufferTraits<T>>
    Item(const T& t)
        : Item(Traits::data(t),
               Traits::size(t))
    {}

    Item(const void* d,
         size_t s)
        : data(d)
        , size(s)
    {
        if (not may_be_null and data == nullptr)
        {
            throw std::logic_error("data pointer may not be null for this item");
        }

        if (data == nullptr and size > 0)
        {
            throw std::logic_error("dazed and confused: data is nullptr but size is > 0");
        }

    }

    ~Item() = default;

    Item(const Item&) = default;

    Item&
    operator=(const Item&) = default;


    const void* data;
    const size_t size;
};

typedef Item<true> Value;
typedef Item<false> Key;

struct Record
{
    Record(const Key& k,
           const Value& v)
        : key(k)
        , val(v)
    {}

    ~Record() = default;

    Record(const Record&) = default;

    Record&
    operator=(const Record&) = default;

    const Key key;
    const Value val;
};

struct TableCounters
{
    uint64_t total_tlogs_read = 0;
    uint64_t incremental_updates = 0;
    uint64_t full_rebuilds = 0;

    bool
    operator==(const TableCounters& other) const
    {
        return
            total_tlogs_read == other.total_tlogs_read and
            incremental_updates == other.incremental_updates and
            full_rebuilds == other.full_rebuilds;
    }

    bool
    operator!=(const TableCounters& other) const
    {
        return not operator==(other);
    }
};

std::ostream&
operator<<(std::ostream&,
           const TableCounters&);

enum class Role
{
    Master,
    Slave
};

// A table represents a namespace within a database.
//
// This is modelled after RocksDB's API, which returns values as `std::string's.
// Which is a big bummer as that means an extra copy in our use case. `unique_ptr's
// or sth. similar that can relinquish ownership of the buffer would've been much nicer.
// So callers will have to be updated if we want to avoid copies.
class TableInterface
{
public:
    virtual ~TableInterface() = default;

    typedef std::vector<Record> Records;

    // allows deletion by setting a Record's value to `None' (nullptr).
    virtual void
    multiset(const Records&,
             Barrier,
             volumedriver::OwnerTag) = 0;

    typedef boost::optional<std::string> MaybeString;
    typedef std::vector<MaybeString> MaybeStrings;
    typedef std::vector<Key> Keys;

    virtual MaybeStrings
    multiget(const Keys& keys) = 0;

    using RelocationLogs = std::vector<std::string>;

    virtual void
    apply_relocations(const volumedriver::ScrubId&,
                      const volumedriver::SCOCloneID,
                      const RelocationLogs&) = 0;

    virtual void
    clear(volumedriver::OwnerTag) = 0;

    // default; specific implementations might want to override it.
    virtual Role
    get_role() const
    {
        return role_;
    }

    // default; specific implementations might want to override it.
    virtual void
    set_role(Role role,
             volumedriver::OwnerTag owner_tag)
    {
        // TODO AR: locking?
        role_ = role;
        owner_tag_ = owner_tag;
    }

    // default; specific implementations might want to override it.
    virtual volumedriver::OwnerTag
    owner_tag() const
    {
        return owner_tag_;
    }

    virtual const std::string&
    nspace() const = 0;

    // Catch up with the backend - returns the number of TLogs that were / have to be
    // replayed.
    // XXX: return an actual list of TLogs (actually pairs of clone ID or namespace and
    // tlog names)?
    virtual size_t
    catch_up(volumedriver::DryRun) = 0;

    virtual TableCounters
    get_counters(volumedriver::Reset) = 0;


private:
    Role role_ = Role::Slave;
    volumedriver::OwnerTag owner_tag_ = volumedriver::OwnerTag(0);
};

typedef std::shared_ptr<TableInterface> TableInterfacePtr;

struct DataBaseInterface
{
    virtual ~DataBaseInterface() = default;

    // creates table if necessary and returns a handle to it
    virtual TableInterfacePtr
    open(const std::string& nspace) = 0;

    // dropped tables remain accessible through open handles!
    virtual void
    drop(const std::string& nspace) = 0;

    virtual std::vector<std::string>
    list_namespaces() = 0;
};

typedef std::shared_ptr<DataBaseInterface> DataBaseInterfacePtr;

}

#endif // !META_DATA_SERVER_INTERFACE_H_
