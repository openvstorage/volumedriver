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

#ifndef ARAKOON_INTERFACE_H
#define ARAKOON_INTERFACE_H

#include "ArakoonNodeConfig.h"
#include "Assert.h"
#include "Catchers.h"
#include "Logging.h"
#include "Tracer.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <exception>

#include <boost/chrono.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/optional.hpp>

#include <arakoon-1.0/arakoon/arakoon.h>

namespace arakoon
{

template<typename T>
struct DataBufferTraits
{};

// Subtle difference between Empty (0 size, non null pointer)
// and None (any size, 0 pointer)
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

class ArakoonEmpty
{};

template<>
struct DataBufferTraits<ArakoonEmpty>
{
    static size_t
    size(const ArakoonEmpty&)
    {
        return 0;
    }

    static const void*
    data(const ArakoonEmpty&)
    {
        // A small carmichael number
        return (void*)561;
    }
};

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

    static std::string
    deserialize(void* data,
                size_t size)
    {
        std::string s(static_cast<const char*>(data), size);
        free(data);
        return s;
    }
};

template<>
struct DataBufferTraits<std::vector<uint8_t> >
{
    static size_t
    size(const std::vector<uint8_t>& in)
    {
        return in.size();
    }

    static const void*
    data(const std::vector<uint8_t>& in)
    {
        return static_cast<const void*>(&in[0]);
    }
};

struct logger
{
    static void
    setLogging()
    {
        arakoon_log_set_handler(&log_handler);
    }

    static void
    setNoLogging()
    {
        arakoon_log_set_handler(&no_log_handler);
    }

    static void
    log_handler(ArakoonLogLevel level,
                const char* message);

    static void
    no_log_handler(ArakoonLogLevel,
                   const char*);

    DECLARE_LOGGER("crakoon");
};

typedef std::pair<size_t, const void*> arakoon_buffer;
template<>
struct DataBufferTraits<arakoon_buffer>
{
    static size_t
    size(const arakoon_buffer& buf)
    {
        return buf.first;
    }

    static const void*
    data(const arakoon_buffer& buf)
    {
        return buf.second;
    }
};

class buffer
{
public:
    buffer();

    buffer(void * data,
           std::size_t const size);

    buffer(const size_t);

    buffer(buffer&&);

    ~buffer() throw();

    buffer(const buffer &) = delete;

    buffer&
    operator=(const buffer &) = delete;

    buffer&
    operator=(buffer&&);

    std::pair<size_t, void*>
    release();

    void*
    data()
    {
        return data_;
    }

    void const *
    data() const
    {
        return data_;
    }

    std::size_t
    size() const
    {
        return size_;
    }

    template<typename R>
    R
    as_istream(std::function<R(std::istream& is)>&& fun) const
    {
        boost::iostreams::array_source src(static_cast<const char*>(data_),
                                           size_);

        boost::iostreams::stream<decltype(src)> is(src);
        return fun(is);
    }

private:
    DECLARE_LOGGER("ArakoonBuffer");

    void* data_;
    std::size_t size_;
};

template<>
struct DataBufferTraits<buffer>
{
    static size_t
    size(const buffer& in)
    {
        return in.size();
    }

    static const void*
    data(const buffer& in)
    {
        return in.data();
    }

    static buffer
    deserialize(void* data,
                size_t size)
    {
        return buffer(data, size);
    }
};

typedef arakoon_rc rc;

void
rc_to_error(rc const rc,
            const std::string& = std::string());

class error
    : public std::exception
{
public:

    explicit error(const std::string& buf)
        : str_(buf)
    {}

    virtual ~error() throw ()
    {};

    virtual rc rc_get() const = 0;

    const std::string& getString() const
    {
        return str_;
    }

    virtual const char*
    what() const throw()
    {
        return arakoon_strerror(rc_get());
    }

protected:
    const std::string str_;
};

template<rc ret_code>
class error_ : public error
{
public:
    error_() = default;

    explicit error_(const std::string& buffer)
        : error(buffer)
    {}

    virtual rc rc_get() const
    {
        return ret_code;
    }

    error_(error_ const&) = default;
    error_& operator=(error_ const &) = delete;
};

typedef error_<ARAKOON_RC_NO_MAGIC> error_no_magic;
typedef error_<ARAKOON_RC_TOO_MANY_DEAD_NODES> error_too_many_dead_nodes;
typedef error_<ARAKOON_RC_NO_HELLO> error_no_hello;
typedef error_<ARAKOON_RC_NOT_MASTER> error_not_master;
typedef error_<ARAKOON_RC_NOT_FOUND> error_not_found;
typedef error_<ARAKOON_RC_WRONG_CLUSTER> error_wrong_cluster;
typedef error_<ARAKOON_RC_NURSERY_RANGE_ERROR> error_nursery_range_error;
typedef error_<ARAKOON_RC_ASSERTION_FAILED> error_assertion_failed;
typedef error_<ARAKOON_RC_READ_ONLY> error_read_only;
typedef error_<ARAKOON_RC_UNKNOWN_FAILURE> error_unknown_failure;

typedef error_<ARAKOON_RC_CLIENT_NETWORK_ERROR> error_client_network_error;
typedef error_<ARAKOON_RC_CLIENT_UNKNOWN_NODE> error_client_unknown_node;
typedef error_<ARAKOON_RC_CLIENT_MASTER_NOT_FOUND> error_client_master_not_found;
typedef error_<ARAKOON_RC_CLIENT_NOT_CONNECTED> error_client_not_connected;
typedef error_<ARAKOON_RC_CLIENT_TIMEOUT> error_client_timeout;
typedef error_<ARAKOON_RC_CLIENT_NURSERY_INVALID_ROUTING> error_client_nursery_invalid_routing;
typedef error_<ARAKOON_RC_CLIENT_NURSERY_INVALID_CONFIG> error_client_nursery_invalid_config;

// Should be renamed since it holds keys or values
class value_list
{
public:
    class iterator
    {
    public:
        iterator(ArakoonValueListIter * const iter);

        ~iterator();

        void
        reset();

        bool
        next(arakoon_buffer&);

        iterator(iterator&&);

    private:
        DECLARE_LOGGER("ArakoonValueListIterator");

        iterator(iterator const &) = delete;
        iterator & operator=(iterator const &) = delete;

        ArakoonValueListIter * iter_;
    };

    explicit value_list(ArakoonValueList * const list  = arakoon_value_list_new());

    ~value_list();

    template<typename T,
             typename Traits = DataBufferTraits<T> >
    void add(const T& value)
    {
        rc_to_error(arakoon_value_list_add(list_,
                                           Traits::size(value),
                                           Traits::data(value)));
    }

    iterator
    begin() const;

    const ArakoonValueList*
    get() const
    {
        return list_;
    }

    size_t
    size() const;

    value_list(value_list &&);
    value_list(const value_list &) = delete;
    value_list & operator=(value_list const &) = delete;
private:
    ArakoonValueList * list_;
};

class key_value_list
{
public:
    class iterator
    {
    public:
        iterator(ArakoonKeyValueListIter * const iter);

        iterator(iterator&&);

        ~iterator();

        void reset();

        bool
        next(arakoon_buffer& key,
             arakoon_buffer& value);

    private:
        DECLARE_LOGGER("ArakoonKeyValueListIterator");

        iterator(iterator const &) = delete;
        iterator & operator=(iterator const &) = delete;

        ArakoonKeyValueListIter * iter_;
    };

    explicit key_value_list(ArakoonKeyValueList * const list);

    key_value_list(key_value_list&&);

    ~key_value_list();

    iterator
    begin() const;

    const ArakoonKeyValueList*
    get() const
    {
        return list_;
    }

    size_t size() const;

    key_value_list(const key_value_list &) = delete;
    key_value_list & operator=(key_value_list const &) = delete;

private:
    ArakoonKeyValueList * list_;
};

class sequence
{
public:
    sequence();

    ~sequence();

    sequence(sequence&&) = default;

    sequence(sequence const &) = delete;

    sequence&
    operator=(sequence const &) = delete;

    template<typename K,
             typename V = ArakoonEmpty,
             typename KTraits = DataBufferTraits<K>,
             typename VTraits = DataBufferTraits<V> >
    void
    add_set(const K& key,
            const V& value = ArakoonEmpty())
    {
        rc_to_error(arakoon_sequence_add_set(sequence_,
                                             KTraits::size(key),
                                             KTraits::data(key),
                                             VTraits::size(value),
                                             VTraits::data(value)));
    }

    template<typename T,
             typename Traits = DataBufferTraits<T> >
    void
    add_delete(const T& key)
    {
        rc_to_error(arakoon_sequence_add_delete(sequence_,
                                                Traits::size(key),
                                                Traits::data(key)));
    }

    template<typename K,
             typename V,
             typename KTraits = DataBufferTraits<K>,
             typename VTraits = DataBufferTraits<V> >
    void
    add_assert(const K& key,
               const V& value)
    {
        rc_to_error(arakoon_sequence_add_assert(sequence_,
                                                KTraits::size(key),
                                                KTraits::data(key),
                                                VTraits::size(value),
                                                VTraits::data(value)));
    }

    const ArakoonSequence*
    get() const
    {
        return sequence_;
    }

private:
    ArakoonSequence * sequence_;
};

class Cluster
{
public:
    using MilliSeconds = boost::chrono::milliseconds;
    using MaybeMilliSeconds = boost::optional<MilliSeconds>;

    template<typename Nodes>
    Cluster(const ClusterID& cluster_id,
            const Nodes& nodes,
            const MaybeMilliSeconds& mms = boost::none,
            ArakoonProtocolVersion version = ARAKOON_PROTOCOL_VERSION_1)
        : cluster_(arakoon_cluster_new(version, cluster_id.c_str()))
        , options_(arakoon_client_call_options_new())
        , cluster_id_(cluster_id)
        , proto_version_(version)
        , configs_(nodes.begin(), nodes.end())
    {
        try
        {
            VERIFY(not cluster_id_.str().empty());

            if (not cluster_)
            {
                LOG_ERROR("Failed to create cluster " << cluster_id_ <<
                          ", proto version " << proto_version_);
                throw std::bad_alloc();
            }

            if (not options_)
            {
                LOG_ERROR("Failed to allocate ArakoonClientCallOptions");
                throw std::bad_alloc();
            }

            LOG_INFO("Cluster ID " << cluster_id_ << ", proto version " << proto_version_);

            timeout(mms);
            add_nodes_();
            connect_master_();
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR("failed to create Arakoon client: " << EWHAT);
                cleanup_();
                throw;
            });
    }

    ~Cluster();

    Cluster(Cluster const &) = delete;

    Cluster&
    operator=(Cluster const &) = delete;

    ClusterID
    cluster_id() const
    {
        return cluster_id_;
    }

    std::string
    hello(const std::string& client_id);

    boost::optional<std::string>
    who_master();

    bool
    expect_progress_possible();

    template<typename T,
             typename Traits= DataBufferTraits<T> >
    bool
    exists(const T& key)
    {
        //  TRACER;

        arakoon_bool result;

        maybe_retry_(arakoon_exists,
                     Traits::size(key),
                     Traits::data(key),
                     &result);

        return (result == ARAKOON_BOOL_TRUE);
    }

    template<typename K,
             typename V = buffer,
             typename KTraits = DataBufferTraits<K>,
             typename VTraits = DataBufferTraits<V> >
    V
    get(const K& key)
    {
        size_t size = 0;
        void* data = nullptr;

        maybe_retry_(arakoon_get,
                     KTraits::size(key),
                     KTraits::data(key),
                     &size,
                     &data);

        return VTraits::deserialize(data, size);
    }

    template<typename K,
             typename V,
             typename KTraits = DataBufferTraits<K>,
             typename VTraits = DataBufferTraits<V> >
    void set(const K& key,
             const V& value = ArakoonEmpty())
    {
        // Bit too much even for me.
        //        TRACER;

        maybe_retry_(arakoon_set,
                     KTraits::size(key),
                     KTraits::data(key),
                     VTraits::size(value),
                     VTraits::data(value));
    }

    template<typename A,
             typename R,
             typename ATraits = DataBufferTraits<A>,
             typename RTraits = DataBufferTraits<R> >
    R
    user_function(const char* fun,
                  const A& arg)
    {
        LOG_TRACE(fun);

        size_t size = 0;
        void* data = nullptr;

        maybe_retry_(arakoon_user_function,
                     fun,
                     ATraits::size(arg),
                     ATraits::data(arg),
                     &size,
                     &data);

        return RTraits::deserialize(data, size);
    }

    template<typename T,
             typename Traits = DataBufferTraits<T> >
    void
    remove(const T& key)
    {
        TRACER;

        maybe_retry_(arakoon_delete,
                     Traits::size(key),
                     Traits::data(key));
    }

    value_list
    multi_get(const value_list & keys);

    template<typename T1,
             typename T2,
             typename Traits1 = DataBufferTraits<T1>,
             typename Traits2 = DataBufferTraits<T2> >
    value_list
    range(const T1& begin_key,
          const bool begin_key_included,
          const T2& end_key,
          const bool end_key_included,
          const ssize_t max_elements)
    {
        TRACER;

        ArakoonValueList* result = nullptr;

        maybe_retry_(&arakoon_range,
                     Traits1::size(begin_key),
                     Traits1::data(begin_key),
                     ToArakoon(begin_key_included),
                     Traits2::size(end_key),
                     Traits2::data(end_key),
                     ToArakoon(end_key_included),
                     max_elements,
                     &result);

        return value_list(result);
    }

    template<typename T1,
             typename T2,
             typename Traits1 = DataBufferTraits<T1>,
             typename Traits2 = DataBufferTraits<T2> >
    key_value_list
    range_entries(const T1& begin_key,
                  const bool begin_key_included,
                  const T2& end_key,
                  const bool end_key_included,
                  const ssize_t max_elements)
    {
        TRACER;

        ArakoonKeyValueList* result;

        maybe_retry_(arakoon_range_entries,
                     Traits1::size(begin_key),
                     Traits1::data(begin_key),
                     ToArakoon(begin_key_included),
                     Traits2::size(end_key),
                     Traits2::data(end_key),
                     ToArakoon(end_key_included),
                     max_elements,
                     &result);

        return key_value_list(result);
    }

    template<typename T1,
             typename T2,
             typename Traits1 = DataBufferTraits<T1>,
             typename Traits2 = DataBufferTraits<T2> >
    key_value_list
    rev_range_entries(const T1 & begin_key,
                      const bool begin_key_included,
                      const T2 & end_key,
                      const bool end_key_included,
                      const ssize_t max_elements)
    {
        TRACER;

        ArakoonKeyValueList * result;

        maybe_retry_(arakoon_rev_range_entries,
                     Traits1::size(begin_key),
                     Traits1::data(begin_key),
                     ToArakoon(begin_key_included),
                     Traits2::size(end_key),
                     Traits2::data(end_key),
                     ToArakoon(end_key_included),
                     max_elements,
                     &result);

        return key_value_list(result);
    }

    template<typename T,
             typename Traits = DataBufferTraits<T> >
    value_list
    prefix(const T& begin_key,
           const ssize_t max_elements = -1)
    {
        TRACER;

        ArakoonValueList* result;

        maybe_retry_(arakoon_prefix,
                     Traits::size(begin_key),
                     Traits::data(begin_key),
                     max_elements,
                     &result);

        return value_list(result);
    }

    uint64_t
    delete_prefix(const std::string& prefix,
                  bool try_old_method = false);

    template<typename K,
             typename VOld,
             typename VNew,
             typename KTraits = DataBufferTraits<K>,
             typename VOldTraits = DataBufferTraits<VOld>,
             typename VNewTraits = DataBufferTraits<VNew> >
    buffer
    test_and_set(const K & key,
                 const VOld & old_value,
                 const VNew & new_value)
    {
        TRACER;

        size_t size = 0;
        void* data = NULL;

        maybe_retry_(arakoon_test_and_set,
                     KTraits::size(key),
                     KTraits::data(key),
                     VOldTraits::size(old_value),
                     VOldTraits::data(old_value),
                     VNewTraits::size(new_value),
                     VNewTraits::data(new_value),
                     &size,
                     &data);

        return buffer(data, size);
    }

    void
    sequence(const arakoon::sequence& sequence);

    void
    synced_sequence(const arakoon::sequence& sequence);

    MaybeMilliSeconds
    timeout() const;

    void
    timeout(const MaybeMilliSeconds& timeout_ms);

    bool
    allow_dirty() const;

    void
    allow_dirty(const bool allow_dirty);

    using NodeConfigs = std::vector<ArakoonNodeConfig>;

    const NodeConfigs&
    configs() const
    {
        return configs_;
    }

private:
    DECLARE_LOGGER("Cluster");

    ArakoonCluster* cluster_;
    ArakoonClientCallOptions* options_;

    const ClusterID cluster_id_;
    const ArakoonProtocolVersion proto_version_;
    const NodeConfigs configs_;

    explicit Cluster(const ClusterID& cluster_name,
                     ArakoonProtocolVersion version = ARAKOON_PROTOCOL_VERSION_1);

    void
    connect_master_();

    inline arakoon_bool
    ToArakoon(bool in)
    {
        return in ? ARAKOON_BOOL_TRUE : ARAKOON_BOOL_FALSE;
    }

    bool
    FromArakoon(arakoon_bool in)
    {
        return in == ARAKOON_BOOL_TRUE;
    }

    void
    add_node_(const ArakoonNodeConfig&);

    void
    add_nodes_();

    void
    reset_();

    void
    cleanup_();

    void
    handle_errors_(arakoon_rc rc);

    template<typename... A>
    void
    maybe_retry_(arakoon_rc (*arakoon_fun)(ArakoonCluster*,
                                           const ArakoonClientCallOptions*,
                                           A... args),
                 A... args)
    {
        try
        {
            handle_errors_(arakoon_fun(cluster_,
                                       options_,
                                       args...));
            return;
        }
        catch (error_not_master& e)
        {
            LOG_WARN(e.what());
        }
        catch (error_client_not_connected& e)
        {
            LOG_WARN(e.what());
        }
        catch (error_client_network_error& e)
        {
            LOG_WARN(e.what());
        }
        catch (error_not_found& e)
        {
            // we typically don't want to see this one
            LOG_DEBUG(e.what());
            throw;
        }
        CATCH_STD_ALL_LOG_RETHROW("arakoon error");

        reset_();

        LOG_WARN(cluster_id_ <<
                 ": trying to reconnect to master before making another attempt");

        connect_master_();

        LOG_WARN(cluster_id_ << ": retrying");

        handle_errors_(arakoon_fun(cluster_,
                                   options_,
                                   args...));
    }
};

} // namespace arakoon

#endif // ARAKOON_INTERFACE_H

// Local Variables: **
// mode: c++ **
// End: **
