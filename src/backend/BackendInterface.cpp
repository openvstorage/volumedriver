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

#include "BackendConfig.h"
#include "BackendConnectionManager.h"
#include "BackendException.h"
#include "BackendInterface.h"
#include "BackendTracePoints_tp.h"

#include <youtils/ScopeExit.h>

namespace backend
{

namespace fs = boost::filesystem;
namespace yt = youtils;

BackendInterface::BackendInterface(const Namespace& nspace,
                                   BackendConnectionManagerPtr conn_manager,
                                   boost::posix_time::time_duration timeout,
                                   unsigned retries)
    : nspace_(nspace)
    , conn_manager_(conn_manager)
    , timeout_(timeout)
    , retries_(retries)
{
}

template<typename ReturnType,
         typename... Args>
ReturnType
BackendInterface::do_wrap_(ReturnType
                           (BackendConnectionInterface::*mem_fun)(Args...),
                           Args... args)
{
    unsigned retries = 0;
    while (true)
    {
        BackendConnectionInterfacePtr
            conn(conn_manager_->getConnection(retries ?
                                              ForceNewConnection::T :
                                              ForceNewConnection::F));
        conn->timeout(timeout_);

        if (retries != 0)
        {
            LOG_WARN("Retrying with new connection (retry: " <<
                     retries << ")");
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
            if (retries++ >= retries_)
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
BackendInterface::wrap_(ReturnType
                        (BackendConnectionInterface::*mem_fun)(const Namespace&,
                                                               Args...),
                        Args... args)
{
    return do_wrap_<ReturnType,
                    const Namespace&,
                    Args...>(mem_fun,
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

    LOG_INFO(nspace_ << ": caller wants latest version: " << insist_on_latest <<
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
                       InsistOnLatestVersion insist_on_latest)
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
                       decltype(insist_on_latest)>(&BackendConnectionInterface::read,
                                                   dst,
                                                   name,
                                                   insist);
             });

    handle_eventual_consistency_<void>(insist_on_latest, std::move(fun));
}

void
BackendInterface::write(const fs::path& src,
                        const std::string& name,
                        const OverwriteObject overwrite,
                        const yt::CheckSum* chksum)
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
          decltype(chksum)>(&BackendConnectionInterface::write,
                            src,
                            name,
                            overwrite,
                            chksum);
}

yt::CheckSum
BackendInterface::getCheckSum(const std::string& name)
{
    return wrap_<yt::CheckSum,
                 decltype(name)>(&BackendConnectionInterface::getCheckSum,
                                 name);
}

bool
BackendInterface::namespaceExists()
{
    return wrap_<bool>(&BackendConnectionInterface::namespaceExists);
}

void
BackendInterface::createNamespace(const NamespaceMustNotExist must_not_exist)
{
    try
    {
        wrap_<void,
              NamespaceMustNotExist>(&BackendConnectionInterface::createNamespace,
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
BackendInterface::deleteNamespace()
{
    wrap_<void>(&BackendConnectionInterface::deleteNamespace);
}

void
BackendInterface::clearNamespace()
{
    wrap_<void>(&BackendConnectionInterface::clearNamespace);
}

void
BackendInterface::invalidate_cache()
{
    do_wrap_<void,
             const boost::optional<Namespace>&>(&BackendConnectionInterface::invalidate_cache,
                                                nspace_);
}

bool
BackendInterface::objectExists(const std::string& name)
{
    return wrap_<bool,
                 decltype(name)>(&BackendConnectionInterface::objectExists,
                                 name);

}

void
BackendInterface::remove(const std::string& name,
                         const ObjectMayNotExist may_not_exist)
{
    wrap_<void,
          decltype(name),
          ObjectMayNotExist>(&BackendConnectionInterface::remove,
                             name,
                             may_not_exist);
}

void
BackendInterface::partial_read(const BackendConnectionInterface::PartialReads& partial_reads,
                               BackendConnectionInterface::PartialReadFallbackFun& fallback_fun,
                               InsistOnLatestVersion insist_on_latest)
{
    size_t bytes = 0;
    for (const auto& p : partial_reads)
    {
        bytes += p.size;
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
                       decltype(fallback_fun)>(&BackendConnectionInterface::partial_read,
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
                                                    conn_manager_,
                                                    timeout_,
                                                    retries_));
}

uint64_t
BackendInterface::getSize(const std::string& name)
{
    return wrap_<uint64_t,
                 decltype(name)>(&BackendConnectionInterface::getSize,
                                 name);
}

void
BackendInterface::listObjects(std::list<std::string>& out)
{
    return wrap_<void,
                 decltype(out)>(&BackendConnectionInterface::listObjects,
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
                         InsistOnLatestVersion insist_on_latest)
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
                              decltype(insist)>(static_cast<fn_type>(&BackendConnectionInterface::x_read),
                                                dst,
                                                name,
                                                insist);
             });

    return handle_eventual_consistency_<ObjectInfo>(insist_on_latest, std::move(fun));
}

ObjectInfo
BackendInterface::x_read(std::string& dst,
                         const std::string& name,
                         InsistOnLatestVersion insist_on_latest)
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
                              decltype(insist)>(static_cast<fn_type>(&BackendConnectionInterface::x_read),
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
                          const yt::CheckSum* chksum)
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
                 decltype(chksum)>(static_cast<fn_type>(&BackendConnectionInterface::x_write),
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
