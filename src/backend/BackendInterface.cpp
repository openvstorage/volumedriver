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

#include "BackendConfig.h"
#include "BackendConnectionManager.h"
#include "BackendException.h"
#include "BackendInterface.h"
#include "BackendRequestParameters.h"
#include "BackendTracePoints_tp.h"
#include "NamespacePoolSelector.h"
#include "PartialReadCounter.h"
#include "RoundRobinPoolSelector.h"

#include <boost/chrono.hpp>
#include <boost/thread.hpp>

#include <youtils/ScopeExit.h>

namespace backend
{

namespace fs = boost::filesystem;
namespace yt = youtils;

BackendInterface::BackendInterface(const Namespace& nspace,
                                   BackendConnectionManagerPtr conn_manager)
    : nspace_(nspace)
    , conn_manager_(conn_manager)
    , retry_counter_(0)
{}

const BackendRequestParameters&
BackendInterface::default_request_parameters()
{
    static const BackendRequestParameters params;
    return params;
}

template<typename PoolSelector,
         typename ReturnType,
         typename... Args>
ReturnType
BackendInterface::do_wrap_(PoolSelector& selector,
                           const BackendRequestParameters& params,
                           ReturnType
                           (BackendConnectionInterface::*mem_fun)(Args...),
                           Args... args)
{
    unsigned attempt = 0;
    const uint32_t retries_on_error =  params.retries_on_error_ ?
        *params.retries_on_error_ :
        conn_manager_->retries_on_error();
    boost::chrono::milliseconds msecs = params.retry_interval_ ?
        *params.retry_interval_ :
        conn_manager_->retry_interval();

    while (true)
    {
        if (attempt != 0)
        {
            ++retry_counter_;
            LOG_WARN(nspace_ << ": retrying with new connection (retry: " <<
                     attempt << ", sleep before retry: " << msecs << ")");
            boost::this_thread::sleep_for(msecs);
            msecs *= params.retry_backoff_multiplier_ ?
                *params.retry_backoff_multiplier_ :
                conn_manager_->retry_backoff_multiplier();
        }

        try
        {
            BackendConnectionInterfacePtr
                conn(selector.pool()->get_connection(attempt == 0 ?
                                                     ForceNewConnection::F :
                                                     ForceNewConnection::T));

            return ((conn.get())->*mem_fun)(std::forward<Args>(args)...);
        }
        catch (BackendAssertionFailedException&)
        {
            throw; /* ... no need to retry. */
        }
        catch (BackendNamespaceDoesNotExistException&)
        {
            throw; /* ... no need to retry. */
        }
        catch (BackendObjectDoesNotExistException&)
        {
            throw; /* ... no need to retry. */
        }
        catch (BackendConnectionTimeoutException& e)
        {
            LOG_ERROR(nspace_ << ": connection timeout");
            selector.request_timeout();

            if (attempt++ >= retries_on_error)
            {
                LOG_ERROR(nspace_ << ": giving up");
                throw;
            }
        }
        catch (BackendConnectFailureException& e)
        {
            LOG_ERROR(nspace_ << ": connection failure");
            selector.connection_error();

            if (attempt++ >= retries_on_error)
            {
                LOG_ERROR(nspace_ << ": giving up");
                throw;
            }
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR(nspace_ << ": problem with connection: " << EWHAT);
                selector.backend_error();

                if (attempt++ >= retries_on_error)
                {
                    LOG_ERROR(nspace_ << ": giving up");
                    throw;
                }
            });
    }
}

namespace
{

struct FixedPoolSelector
{
    std::shared_ptr<ConnectionPool> pool_;

    FixedPoolSelector(const std::shared_ptr<ConnectionPool>& p)
        : pool_(p)
    {}

    const std::shared_ptr<ConnectionPool>&
    pool() const
    {
        return pool_;
    }

    void
    connection_error()
    {
        pool_->error();
    }

    void
    backend_error()
    {}

    void
    request_timeout()
    {}
};

}

template<typename ReturnType,
         typename... Args>
ReturnType
BackendInterface::wrap_(const BackendRequestParameters& params,
                        ReturnType
                        (BackendConnectionInterface::*mem_fun)(const Namespace&,
                                                               Args...),
                        Args... args)
{
    const SwitchConnectionPoolPolicy policy =
        conn_manager_->connection_manager_parameters().backend_interface_switch_connection_pool_policy.value();
    switch (policy)
    {
    case SwitchConnectionPoolPolicy::OnError:
        {
            NamespacePoolSelector selector(*conn_manager_,
                                           nspace_);

            return do_wrap_<NamespacePoolSelector,
                            ReturnType,
                            const Namespace&,
                            Args...>(selector,
                                     params,
                                     mem_fun,
                                     nspace_,
                                     std::forward<Args>(args)...);
        }
    case SwitchConnectionPoolPolicy::RoundRobin:
        {
            RoundRobinPoolSelector selector(*conn_manager_);

            return do_wrap_<RoundRobinPoolSelector,
                            ReturnType,
                            const Namespace&,
                            Args...>(selector,
                                     params,
                                     mem_fun,
                                     nspace_,
                                     std::forward<Args>(args)...);
        }
    }

    ASSERT(0 == "unknown SwitchConnectionPoolPolicy");

    std::stringstream ss;
    ss << "Unknown SwitchConnectionPoolPolicy " << policy;
    throw fungi::IOException(ss.str());
}

// XXX: The incoming and outgoing values for "InsistOnLatestVersion" might be confusing -
// it could make sense to introduce another type for either of these.
template<typename R>
R
BackendInterface::handle_eventual_consistency_(InsistOnLatestVersion insist_on_latest,
                                               std::function<R(InsistOnLatestVersion)>&& fun)
{
    const StrictConsistency consistency(conn_manager_->config().strict_consistency());
    const InsistOnLatestVersion insist =
        consistency == StrictConsistency::T ?
        InsistOnLatestVersion::F :
        insist_on_latest;

    LOG_DEBUG(nspace_ << ": caller wants latest version: " << insist_on_latest <<
              ", backend offers strict consistency: " <<
              (consistency == StrictConsistency::T) <<
              " -> " <<
              ((insist == InsistOnLatestVersion::T) ? "insisting"  : "not insisting") <<
              " on latest version towards the backend");
    try
    {
        return fun(insist);
    }
    catch (BackendObjectDoesNotExistException&)
    {
        if (consistency == StrictConsistency::F and
            insist == InsistOnLatestVersion::F)
        {
            LOG_WARN(nspace_ <<
                     ": object purportedly doesn't exist but the backend doesn't offer strict consistency - trying harder");
            return fun(InsistOnLatestVersion::T);
        }
        else
        {
            throw;
        }
    }
}

void
BackendInterface::read(const fs::path& dst,
                       const std::string& name,
                       InsistOnLatestVersion insist_on_latest,
                       const BackendRequestParameters& params)
{
    tracepoint(openvstorage_backend,
               backend_interface_get_object_start,
               nspace_.str().c_str(),
               name.c_str(),
               dst.string().c_str(),
               insist_on_latest == InsistOnLatestVersion::T);

    auto exit(yt::make_scope_exit([&]
             {
                 tracepoint(openvstorage_backend,
                            backend_interface_get_object_end,
                            nspace_.str().c_str(),
                            name.c_str(),
                            std::uncaught_exception());
             }));

    auto fun([&](InsistOnLatestVersion insist)
             {
                 wrap_<void,
                       decltype(dst),
                       decltype(name),
                       decltype(insist_on_latest)>(params,
                                                   &BackendConnectionInterface::read,
                                                   dst,
                                                   name,
                                                   insist);
             });

    handle_eventual_consistency_<void>(insist_on_latest,
                                       std::move(fun));
}

void
BackendInterface::write(const fs::path& src,
                        const std::string& name,
                        const OverwriteObject overwrite,
                        const yt::CheckSum* chksum,
                        const boost::shared_ptr<Condition>& cond,
                        const BackendRequestParameters& params)
{
    tracepoint(openvstorage_backend,
               backend_interface_put_object_start,
               nspace_.str().c_str(),
               name.c_str(),
               src.string().c_str(),
               overwrite == OverwriteObject::T);

    auto exit(yt::make_scope_exit([&]
             {
                 tracepoint(openvstorage_backend,
                            backend_interface_put_object_end,
                            nspace_.str().c_str(),
                            name.c_str(),
                            std::uncaught_exception());
             }));

    wrap_<void,
          decltype(src),
          decltype(name),
          OverwriteObject,
          decltype(chksum),
          decltype(cond)>(params,
                          &BackendConnectionInterface::write,
                          src,
                          name,
                          overwrite,
                          chksum,
                          cond);
}

yt::CheckSum
BackendInterface::getCheckSum(const std::string& name,
                              const BackendRequestParameters& params)
{
    return wrap_<yt::CheckSum,
                 decltype(name)>(params,
                                 &BackendConnectionInterface::getCheckSum,
                                 name);
}

bool
BackendInterface::namespaceExists(const BackendRequestParameters& params)
{
    return wrap_<bool>(params,
                       &BackendConnectionInterface::namespaceExists);
}

void
BackendInterface::createNamespace(const NamespaceMustNotExist must_not_exist,
                                  const BackendRequestParameters& params)
{
    try
    {
        wrap_<void,
              NamespaceMustNotExist>(params,
                                     &BackendConnectionInterface::createNamespace,
                                     must_not_exist);
    }
    catch (BackendNamespaceAlreadyExistsException&)
    {
        if (must_not_exist == NamespaceMustNotExist::T)
        {
            throw;
        }
    }
}

void
BackendInterface::deleteNamespace(const BackendRequestParameters& params)
{
    wrap_<void>(params,
                &BackendConnectionInterface::deleteNamespace);
}

void
BackendInterface::clearNamespace(const BackendRequestParameters& params)
{
    wrap_<void>(params,
                &BackendConnectionInterface::clearNamespace);
}

void
BackendInterface::invalidate_cache(const BackendRequestParameters& params)
{
    conn_manager_->visit_pools([&](std::shared_ptr<ConnectionPool> pool)
    {
        FixedPoolSelector selector(pool);

        do_wrap_<FixedPoolSelector,
                 void,
                 const boost::optional<Namespace>&>(selector,
                                                    params,
                                                    &BackendConnectionInterface::invalidate_cache,
                                                    nspace_);
    });
}

bool
BackendInterface::objectExists(const std::string& name,
                               const BackendRequestParameters& params)
{
    return wrap_<bool,
                 decltype(name)>(params,
                                 &BackendConnectionInterface::objectExists,
                                 name);
}

void
BackendInterface::remove(const std::string& name,
                         const ObjectMayNotExist may_not_exist,
                         const boost::shared_ptr<Condition>& cond,
                         const BackendRequestParameters& params)
{
    wrap_<void,
          decltype(name),
          ObjectMayNotExist,
          decltype(cond)>(params,
                          &BackendConnectionInterface::remove,
                          name,
                          may_not_exist,
                          cond);
}

PartialReadCounter
BackendInterface::partial_read(const BackendConnectionInterface::PartialReads& partial_reads,
                               BackendConnectionInterface::PartialReadFallbackFun& fallback_fun,
                               InsistOnLatestVersion insist_on_latest,
                               const BackendRequestParameters& params)
{
    LOG_TRACE(partial_reads << ", " << insist_on_latest);

    size_t bytes = 0;

    for (const auto& p : partial_reads)
    {
        for (const auto& slice : p.second)
        {
            tracepoint(openvstorage_backend,
                       backend_interface_partial_read_descriptor,
                       nspace_.str().c_str(),
                       p.first.c_str(),
                       slice.offset,
                       slice.size,
                       slice.buf);

            bytes += slice.size;
        }
    }

    tracepoint(openvstorage_backend,
               backend_interface_partial_read_start,
               nspace_.str().c_str(),
               partial_reads.size(),
               bytes,
               insist_on_latest == InsistOnLatestVersion::T);

    auto exit(yt::make_scope_exit([&]
                                  {
                                      tracepoint(openvstorage_backend,
                                                 backend_interface_partial_read_end,
                                                 nspace_.str().c_str(),
                                                 partial_reads.size(),
                                                 bytes,
                                                 std::uncaught_exception());
                                  }));

    auto fun([&](InsistOnLatestVersion insist) -> PartialReadCounter
             {
                 return wrap_<PartialReadCounter,
                              decltype(partial_reads),
                              decltype(insist),
                              decltype(fallback_fun)>(params,
                                                      &BackendConnectionInterface::partial_read,
                                                      partial_reads,
                                                      insist,
                                                      fallback_fun);
             });

    if (not conn_manager_->partial_read_nullio())
    {
        return handle_eventual_consistency_<PartialReadCounter>(insist_on_latest,
                                                                std::move(fun));
    }
    else
    {
        return PartialReadCounter();
    }
}

BackendInterfacePtr
BackendInterface::clone() const
{
    return cloneWithNewNamespace(nspace_);
}

BackendInterfacePtr
BackendInterface::cloneWithNewNamespace(const Namespace& new_nspace) const
{
    return BackendInterfacePtr(new BackendInterface(new_nspace,
                                                    conn_manager_));
}

uint64_t
BackendInterface::getSize(const std::string& name,
                          const BackendRequestParameters& params)
{
    return wrap_<uint64_t,
                 decltype(name)>(params,
                                 &BackendConnectionInterface::getSize,
                                 name);
}

void
BackendInterface::listObjects(std::list<std::string>& out,
                              const BackendRequestParameters& params)
{
    return wrap_<void,
                 decltype(out)>(params,
                                &BackendConnectionInterface::listObjects,
                                out);
}

#undef RETRY

const Namespace&
BackendInterface::getNS() const
{
    return nspace_;
}

std::unique_ptr<yt::UniqueObjectTag>
BackendInterface::get_tag(const std::string& name,
                          const BackendRequestParameters& params)
{
    return wrap_<std::unique_ptr<yt::UniqueObjectTag>,
                 decltype(name)>(params,
                                 &BackendConnectionInterface::get_tag,
                                 name);
}

std::unique_ptr<yt::UniqueObjectTag>
BackendInterface::write_tag(const fs::path& src,
                            const std::string& name,
                            const yt::UniqueObjectTag* prev_tag,
                            const OverwriteObject overwrite,
                            const BackendRequestParameters& params)
{
    return wrap_<std::unique_ptr<yt::UniqueObjectTag>,
                 decltype(src),
                 decltype(name),
                 decltype(prev_tag),
                 OverwriteObject>(params,
                                  &BackendConnectionInterface::write_tag,
                                  src,
                                  name,
                                  prev_tag,
                                  overwrite);
}

std::unique_ptr<yt::UniqueObjectTag>
BackendInterface::read_tag(const fs::path& src,
                           const std::string& name,
                           const BackendRequestParameters& params)
{
    return wrap_<std::unique_ptr<yt::UniqueObjectTag>,
                 decltype(src),
                 decltype(name)>(params,
                                 &BackendConnectionInterface::read_tag,
                                 src,
                                 name);
}

bool
BackendInterface::unique_tag_support() const
{
    return conn_manager_->config().unique_tag_support();
}

}

// Local Variables: **
// mode: c++ **
// End: **
