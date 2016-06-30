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
{}

const BackendRequestParameters&
BackendInterface::default_request_parameters()
{
    static const BackendRequestParameters params;
    return params;
}

template<typename ReturnType,
         typename... Args>
ReturnType
BackendInterface::do_wrap_(const BackendRequestParameters& params,
                           ReturnType
                           (BackendConnectionInterface::*mem_fun)(Args...),
                           Args... args)
{
    unsigned retries = 0;
    boost::chrono::milliseconds msecs = params.retry_interval_ ?
        *params.retry_interval_ :
        conn_manager_->retry_interval();

    while (true)
    {
        BackendConnectionInterfacePtr
            conn(conn_manager_->getConnection(retries ?
                                              ForceNewConnection::T :
                                              ForceNewConnection::F));
        conn->timeout(params.timeout_ ?
                      *params.timeout_ :
                      conn_manager_->default_timeout());

        if (retries != 0)
        {
            LOG_WARN("Retrying with new connection (retry: " <<
                     retries << ", sleep before retry: " << msecs << ")");
            boost::this_thread::sleep_for(msecs);
            msecs *= params.retry_backoff_multiplier_ ?
                *params.retry_backoff_multiplier_ :
                conn_manager_->retry_backoff_multiplier();
        }

        LOG_TRACE("Got connection handle " << conn.get());

        try
        {
            return ((conn.get())->*mem_fun)(args...);
        }
        catch (BackendOverwriteNotAllowedException&)
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
        catch (std::exception& e)
        {
            LOG_ERROR("Problem with connection " << conn.get() <<
                      ": " << e.what());
            const uint32_t r = params.retries_on_error_ ?
                *params.retries_on_error_ :
                conn_manager_->retries_on_error();

            if (retries++ >= r)
            {
                LOG_ERROR("Giving up connection " << conn.get());
                throw;
            }
        }
        catch (...)
        {
            LOG_ERROR("Unknown problem with connection " <<
                      conn.get() << " - giving up");
            throw;
        }
    }
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
    return do_wrap_<ReturnType,
                    const Namespace&,
                    Args...>(params,
                             mem_fun,
                             nspace_,
                             args...);
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
          decltype(chksum)>(params,
                            &BackendConnectionInterface::write,
                            src,
                            name,
                            overwrite,
                            chksum);
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
    do_wrap_<void,
             const boost::optional<Namespace>&>(params,
                                                &BackendConnectionInterface::invalidate_cache,
                                                nspace_);
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
                         const BackendRequestParameters& params)
{
    wrap_<void,
          decltype(name),
          ObjectMayNotExist>(params,
                             &BackendConnectionInterface::remove,
                             name,
                             may_not_exist);
}

void
BackendInterface::partial_read(const BackendConnectionInterface::PartialReads& partial_reads,
                               BackendConnectionInterface::PartialReadFallbackFun& fallback_fun,
                               InsistOnLatestVersion insist_on_latest,
                               const BackendRequestParameters& params)
{
    size_t bytes = 0;

    for (const auto& p : partial_reads)
    {
        for (const auto& slice : p.second)
        {
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

    auto fun([&](InsistOnLatestVersion insist)
             {
                 wrap_<void,
                       decltype(partial_reads),
                       decltype(insist),
                       decltype(fallback_fun)>(params,
                                               &BackendConnectionInterface::partial_read,
                                               partial_reads,
                                               insist,
                                               fallback_fun);
             });

    handle_eventual_consistency_<void>(insist_on_latest,
                                       std::move(fun));
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

bool
BackendInterface::hasExtendedApi()
{
    return conn_manager_->getConnection()->hasExtendedApi();
}

ObjectInfo
BackendInterface::x_read(std::stringstream& dst,
                         const std::string& name,
                         InsistOnLatestVersion insist_on_latest,
                         const BackendRequestParameters& params)
{
    typedef ObjectInfo
        (BackendConnectionInterface::*fn_type)(const Namespace&,
                                               std::stringstream&, // dst
                                               const std::string&, // name
                                               InsistOnLatestVersion);

    auto fun([&](InsistOnLatestVersion insist)
             {
                 return wrap_<ObjectInfo,
                              decltype(dst),
                              decltype(name),
                              decltype(insist)>(params,
                                                static_cast<fn_type>(&BackendConnectionInterface::x_read),
                                                dst,
                                                name,
                                                insist);
             });

    return handle_eventual_consistency_<ObjectInfo>(insist_on_latest, std::move(fun));
}

ObjectInfo
BackendInterface::x_read(std::string& dst,
                         const std::string& name,
                         InsistOnLatestVersion insist_on_latest,
                         const BackendRequestParameters& params)
{
    typedef ObjectInfo
        (BackendConnectionInterface::*fn_type)(const Namespace&,
                                               std::string&, // dst
                                               const std::string&, // name
                                               InsistOnLatestVersion);

    auto fun([&](InsistOnLatestVersion insist)
             {
                 return wrap_<ObjectInfo,
                              decltype(dst),
                              decltype(name),
                              decltype(insist)>(params,
                                                static_cast<fn_type>(&BackendConnectionInterface::x_read),
                                                dst,
                                                name,
                                                insist);
             });

    return handle_eventual_consistency_<ObjectInfo>(insist_on_latest,
                                                    std::move(fun));
}

ObjectInfo
BackendInterface::x_write(const std::string& src,
                          const std::string& name,
                          const OverwriteObject overwrite,
                          const backend::ETag* etag,
                          const yt::CheckSum* chksum,
                          const BackendRequestParameters& params)
{
    typedef ObjectInfo
        (BackendConnectionInterface::*fn_type)(const Namespace&,
                                               const std::string&, // src
                                               const std::string&, // name
                                               const OverwriteObject,
                                               const backend::ETag*,
                                               const yt::CheckSum*);

    return wrap_<ObjectInfo,
                 decltype(src),
                 decltype(name),
                 OverwriteObject,
                 decltype(etag),
                 decltype(chksum)>(params,
                                   static_cast<fn_type>(&BackendConnectionInterface::x_write),
                                   src,
                                   name,
                                   overwrite,
                                   etag,
                                   chksum);
}

}

// Local Variables: **
// mode: c++ **
// End: **
