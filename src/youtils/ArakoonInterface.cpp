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

#include "ArakoonInterface.h"
#include "ArakoonNodeConfig.h"
#include "wall_timer.h"

#include <string.h>

#include <stdexcept>

#include <boost/chrono.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/scope_exit.hpp>
#include <boost/thread.hpp>

extern arakoon_rc
arakoon_memory_set_hooks(const ArakoonMemoryHooks * const hooks) ARAKOON_GNUC_NONNULL;

namespace arakoon
{

void
logger::log_handler(ArakoonLogLevel level,
                    const char* message)
{
    switch(level)
    {
    case ARAKOON_LOG_TRACE:
        LOG_TRACE(message);
        break;
    case ARAKOON_LOG_DEBUG:
        LOG_DEBUG(message);
        break;
    case ARAKOON_LOG_INFO:
        LOG_INFO(message);
        break;
    case ARAKOON_LOG_WARNING:
        LOG_WARN(message);
        break;
    case ARAKOON_LOG_ERROR:
        LOG_ERROR(message);
        break;
    case ARAKOON_LOG_FATAL:
        LOG_FATAL(message);
        break;
    }
}

void
logger::no_log_handler(ArakoonLogLevel,
                       const char*)
{}

void
rc_to_error(rc const rc,
            const std::string& buf)
{
    if (ARAKOON_RC_IS_SUCCESS(rc))
    {
        return;
    }

    if (ARAKOON_RC_IS_ERRNO(rc))
    {
        throw std::runtime_error(strerror(ARAKOON_RC_AS_ERRNO(rc)));
    }

    switch ((ArakoonReturnCode) (rc))
    {
    case ARAKOON_RC_SUCCESS:
        return; // should be unreachable code
    case ARAKOON_RC_NO_MAGIC:
        throw error_no_magic(buf);
    case ARAKOON_RC_TOO_MANY_DEAD_NODES:
        throw error_too_many_dead_nodes(buf);
    case ARAKOON_RC_NO_HELLO:
        throw error_no_hello(buf);
    case ARAKOON_RC_NOT_MASTER:
        throw error_not_master(buf);
    case ARAKOON_RC_NOT_FOUND:
        throw error_not_found(buf);
    case ARAKOON_RC_WRONG_CLUSTER:
        throw error_wrong_cluster(buf);
    case ARAKOON_RC_NURSERY_RANGE_ERROR:
        throw error_nursery_range_error(buf);
    case ARAKOON_RC_ASSERTION_FAILED:
        throw error_assertion_failed(buf);
    case ARAKOON_RC_READ_ONLY:
        throw error_read_only(buf);
    case ARAKOON_RC_UNKNOWN_FAILURE:
        throw error_unknown_failure(buf);
    case ARAKOON_RC_CLIENT_NETWORK_ERROR:
        throw error_client_network_error(buf);
    case ARAKOON_RC_CLIENT_UNKNOWN_NODE:
        throw error_client_unknown_node(buf);
    case ARAKOON_RC_CLIENT_MASTER_NOT_FOUND:
        throw error_client_master_not_found(buf);
    case ARAKOON_RC_CLIENT_NOT_CONNECTED:
        throw error_client_not_connected(buf);
    case ARAKOON_RC_CLIENT_TIMEOUT:
        throw error_client_timeout(buf);
    case ARAKOON_RC_CLIENT_NURSERY_INVALID_ROUTING:
        throw error_client_nursery_invalid_routing(buf);
    case ARAKOON_RC_CLIENT_NURSERY_INVALID_CONFIG:
        throw error_client_nursery_invalid_config(buf);
    }
}

//// memory hooks

static ArakoonMemoryHooks memory_hooks = { ::malloc, ::free, ::realloc };

void memory_set_hooks(ArakoonMemoryHooks const * const hooks);
// Have no idea how this is supposed to work with the shared pointer below...
void
memory_set_hooks(ArakoonMemoryHooks const * const hooks)
{
    rc_to_error(arakoon_memory_set_hooks(hooks));

    memcpy(&memory_hooks, hooks, sizeof(ArakoonMemoryHooks));
}

//// buffer

buffer::buffer()
    : data_(nullptr)
    , size_(0)
{}

buffer::buffer(const size_t size)
    : data_(memory_hooks.malloc(size))
    , size_(size)
{
    VERIFY(data_ != nullptr);
}

buffer::buffer(void * data,
               std::size_t const size)
    : data_(data)
    , size_(size)
{
    VERIFY(data_ != nullptr or
           size == 0);
}

buffer::~buffer() throw ()
{
    memory_hooks.free(data_);
}

buffer::buffer(buffer&& other)
{
    data_ = other.data_;
    other.data_ = nullptr;
    size_ = other.size_;
    other.size_ = 0;
}

buffer&
buffer::operator=(buffer&& other)
{
    if (this != &other)
    {
        memory_hooks.free(data_);
        data_ = other.data_;
        other.data_ = nullptr;
        size_ = other.size_;
        other.size_ = 0;
    }
    return *this;
}

std::pair<size_t, void*>
buffer::release()
{
    auto res  = std::make_pair(size_, data_);
    data_ = nullptr ;
    size_ = 0;
    return res;
}

value_list::iterator::iterator(ArakoonValueListIter * const iter)
    : iter_(iter)
{
    if (not iter)
    {
        throw std::invalid_argument("value_list_iterator::value_list_iterator");
    }
}

value_list::iterator::iterator(iterator&& other)
{
    iter_ = other.iter_;
    other.iter_ = nullptr;
}

value_list::iterator::~iterator()
{
    if(iter_)
    {
        arakoon_value_list_iter_free(iter_);
    }
}

void
value_list::iterator::reset()
{
    rc_to_error(arakoon_value_list_iter_reset(iter_));
}

bool
value_list::iterator::next(arakoon_buffer& val)
{
    rc_to_error(arakoon_value_list_iter_next(iter_, &val.first, &val.second));
    return val.second;
}

value_list::value_list(ArakoonValueList * const list)
    : list_(list)
{
    if (not list)
    {
        throw std::invalid_argument("value_list::value_list");
    }
}

value_list::value_list(value_list&& other)
{
    list_ = other.list_;
    other.list_ = nullptr;
}

value_list::~value_list()
{
    if (list_)
    {
        arakoon_value_list_free(list_);
    }
}

value_list::iterator
value_list::begin() const
{
    return iterator(arakoon_value_list_create_iter(list_));
}

size_t
value_list::size() const
{
    ssize_t size = arakoon_value_list_size(list_);
    if (size < 0)
    {
        throw std::runtime_error(strerror(errno));
    }
    return static_cast<size_t>(size);
}

key_value_list::iterator::iterator(ArakoonKeyValueListIter * const iter)
    : iter_(iter)
{
    if (not iter)
    {
        throw std::invalid_argument("key_value_list_iterator::key_value_list_iterator");
    }
}

key_value_list::iterator::~iterator()
{
    if(iter_)
    {
        arakoon_key_value_list_iter_free(iter_);
    }
}

void
key_value_list::iterator::reset()
{
    rc_to_error(arakoon_key_value_list_iter_reset(iter_));
}

bool
key_value_list::iterator::next(arakoon_buffer& key,
                               arakoon_buffer& value)
{
    VERIFY(iter_);
    rc_to_error(arakoon_key_value_list_iter_next(iter_,
                                                 &key.first,
                                                 &key.second,
                                                 &value.first,
                                                 &value.second));
    return key.second;
}

key_value_list::key_value_list(ArakoonKeyValueList * const list)
    : list_(list)
{
    if (not list)
    {
        throw std::invalid_argument("key_value_list::key_value_list");
    }
}

key_value_list::key_value_list(key_value_list&& other)
{
    list_ = other.list_;
    other.list_ = nullptr;
}

key_value_list::~key_value_list()
{
    if (list_)
    {
        arakoon_key_value_list_free(list_);
    }
}

key_value_list::iterator
key_value_list::begin() const
{
    return iterator(arakoon_key_value_list_create_iter(list_));
}

size_t
key_value_list::size() const
{
    ssize_t size = arakoon_key_value_list_size(list_);
    if (size < 0)
    {
        throw std::runtime_error(strerror(errno));
    }

    return (size_t) size;
}

sequence::sequence()
{
    sequence_ = arakoon_sequence_new();
    if (not sequence_)
    {
        throw std::bad_alloc();
    }
}

sequence::~sequence()
{
    arakoon_sequence_free(sequence_);
}

Cluster::~Cluster()
{
    cleanup_();
}

void
Cluster::cleanup_()
{
    arakoon_cluster_free(cluster_);
    arakoon_client_call_options_free(options_);
}

void
Cluster::reset_()
{
    LOG_INFO(cluster_id_ << ": resetting " << cluster_id_);

    arakoon_cluster_free(cluster_);

    cluster_ = arakoon_cluster_new(proto_version_, cluster_id_.str().c_str());
    if (not cluster_)
    {
        LOG_ERROR("Failed to create cluster " << cluster_id_ <<
                  ", proto version " << proto_version_);
        throw std::bad_alloc();
    }

    add_nodes_();
}

void
Cluster::handle_errors_(arakoon_rc rc)
{
    if (not ARAKOON_RC_IS_SUCCESS(rc))
    {
        const void* msg = nullptr;
        size_t size = 0;

        const arakoon_rc rc2 = arakoon_cluster_get_last_error(cluster_,
                                                              &size,
                                                              &msg);
        if (ARAKOON_RC_IS_SUCCESS(rc2) and
            msg and
            size > 0)
        {
            rc_to_error(rc, std::string(static_cast<const char*>(msg), size));
        }
        else
        {
            rc_to_error(rc);
        }
    }
}

void
Cluster::add_node_(const ArakoonNodeConfig& cfg)
{
    const NodeID& name = cfg.node_id_;
    const std::string& host = cfg.hostname_;
    const std::string service = boost::lexical_cast<std::string>(cfg.port_);

    LOG_INFO(cluster_id_ << ": adding arakoon node " << name <<
             ", host " << host << ", service " << service);

    ArakoonClusterNode* node = arakoon_cluster_node_new(name.c_str());
    if (not node)
    {
        LOG_ERROR("Failed to allocate resources for arakoon node " << name);
        throw std::bad_alloc();
    }

    try
    {
        handle_errors_(arakoon_cluster_node_add_address_tcp(node,
                                                            host.c_str(),
                                                            service.c_str()));

        handle_errors_(arakoon_cluster_add_node(cluster_, node));
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("Failed to add arakoon node " << name << ": " << EWHAT);
            arakoon_cluster_node_free(node);
            throw;
        });
}

void
Cluster::add_nodes_()
{
    for (const auto& cfg : configs_)
    {
        add_node_(cfg);
    }
}

bool
Cluster::allow_dirty() const
{
    TRACER;
    return arakoon_client_call_options_get_allow_dirty(options_);
}


void
Cluster::allow_dirty(bool const allow_dirty)
{
    TRACER;
    handle_errors_(arakoon_client_call_options_set_allow_dirty(options_,
                                                               allow_dirty ?
                                                               ARAKOON_BOOL_TRUE :
                                                               ARAKOON_BOOL_FALSE));
}

Cluster::MaybeMilliSeconds
Cluster::timeout() const
{
    TRACER;
    const int ms = arakoon_client_call_options_get_timeout(options_);
    if (ms == ARAKOON_CLIENT_CALL_OPTIONS_INFINITE_TIMEOUT)
    {
        return boost::none;
    }
    else
    {
        return boost::chrono::milliseconds(ms);
    }
}

void
Cluster::timeout(const MaybeMilliSeconds& mms)
{
    TRACER;

    const int ms = mms == boost::none ?
        ARAKOON_CLIENT_CALL_OPTIONS_INFINITE_TIMEOUT :
        mms->count();

    handle_errors_(arakoon_client_call_options_set_timeout(options_,
                                                           ms));
}

void
Cluster::connect_master_()
{
    TRACER;

    using Clock = boost::chrono::high_resolution_clock;

    static_assert(Clock::is_steady,
                  "go on and use another clock then");

    const Clock::time_point start(Clock::now());
    const MaybeMilliSeconds mms(timeout());
    const MilliSeconds ms(mms == boost::none ? 0 : mms->count());

    uint64_t retries = 0;

    while (true)
    {
        try
        {
            handle_errors_(arakoon_cluster_connect_master(cluster_,
                                                          options_));
            return;
        }
        catch (error&)
        {
            Clock::time_point now = Clock::now();
            const auto elapsed(boost::chrono::duration_cast<MilliSeconds>(now - start));

            if (elapsed >= ms)
            {
                LOG_ERROR("Could not connect to Arakoon master after " <<
                          elapsed.count() << " milliseconds (" << retries << " retries)");
                throw;
            }

            boost::this_thread::sleep_for(MilliSeconds(100));
        }
    }
}

std::string
Cluster::hello(const std::string& str)
{
    TRACER;

    char * result = NULL;

    BOOST_SCOPE_EXIT((&result))
    {
        free(result);
    }
    BOOST_SCOPE_EXIT_END;

    handle_errors_(arakoon_hello(cluster_,
                                 options_,
                                 str.c_str(),
                                 cluster_id().c_str(),
                                 &result));

    return std::string(result);
}

boost::optional<std::string>
Cluster::who_master()
{
    TRACER;

    char* result = NULL;

    BOOST_SCOPE_EXIT((&result))
    {
        free(result);
    }
    BOOST_SCOPE_EXIT_END;

    handle_errors_(arakoon_who_master(cluster_,
                                      options_,
                                      &result));
    if (result)
    {
        return std::string(result);
    }
    else
    {
        return boost::none;
    }
}

bool
Cluster::expect_progress_possible()
{
    TRACER;

    arakoon_bool result;
    maybe_retry_<std::add_pointer<decltype(result)>::type>(arakoon_expect_progress_possible,
                                                           &result);
    return (result == ARAKOON_BOOL_TRUE);
}

value_list
Cluster::multi_get(value_list const & keys)
{
    TRACER;

    ArakoonValueList * result = NULL;

    maybe_retry_<decltype(keys.get()),
                 std::add_pointer<decltype(result)>::type>(arakoon_multi_get,
                                                           keys.get(),
                                                           &result);
    return value_list(result);
}

void
Cluster::sequence(arakoon::sequence const & sequence)
{
    TRACER;

    maybe_retry_<decltype(sequence.get())>(arakoon_sequence,
                                           sequence.get());
}

void
Cluster::synced_sequence(arakoon::sequence const & sequence)
{
    TRACER;

    maybe_retry_<decltype(sequence.get())>(arakoon_synced_sequence,
                                           sequence.get());
}

uint64_t
Cluster::delete_prefix(const std::string& prefix,
                         bool /*try_old_method*/)
{
    TRACER;

    uint32_t result = 0;

    maybe_retry_<size_t,
                 const void*,
                 std::add_pointer<decltype(result)>::type>(arakoon_delete_prefix,
                                                           prefix.size(),
                                                           static_cast<const void*>(prefix.c_str()),
                                                           &result);
    return result;
}

}
