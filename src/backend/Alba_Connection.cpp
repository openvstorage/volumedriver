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

#include "Alba_Connection.h"
#include "BackendException.h"

#include <mutex>

#include <boost/optional/optional_io.hpp>

#include <alba/proxy_protocol.h>
#include <alba/checksum.h>
#include <alba/alba_logger.h>

#include <youtils/Assert.h>
#include <youtils/CheckSum.h>

using namespace std;

namespace al = alba::logger;
namespace app = alba::proxy_protocol;
namespace apc = alba::proxy_client;
namespace fs = boost::filesystem;
namespace yt = youtils;

using MaybeString = boost::optional<std::string>;
using AlbaObjectInfo = std::tuple<uint64_t, alba::Checksum*>;

namespace
{

static youtils::Severity
convertAlbaLevel(alba::logger::AlbaLogLevel albaLevel)
{
    switch (albaLevel)
    {
    case alba::logger::AlbaLogLevel::DEBUG:
        {
            return youtils::Severity::debug;
        }
    case alba::logger::AlbaLogLevel::INFO:
        {
            return youtils::Severity::info;
        }
    case alba::logger::AlbaLogLevel::ERROR:
        {
            return youtils::Severity::error;
        }
    case alba::logger::AlbaLogLevel::WARNING:
        {
            return youtils::Severity::warning;
        }
    }
    return youtils::Severity::fatal;
}

void
logMessage(al::AlbaLogLevel albaLevel, string &message)
{
    youtils::Logger::logger_type alba_logger("AlbaProxyClient");
    auto level = convertAlbaLevel(albaLevel);
    if (youtils::Logger::filter(alba_logger.name,
                                level))
    {
        BOOST_LOG_SEV(alba_logger.get(), level) << __FUNCTION__  << ": " << message;
    }
}

}

namespace backend
{

namespace
{

boost::optional<std::string>
extract_preset(const AlbaConfig& cfg)
{
    if (not cfg.alba_connection_preset.value().empty())
    {
        return cfg.alba_connection_preset.value();
    }
    else
    {
        return boost::none;
    }
}

boost::optional<apc::RoraConfig>
extract_rora_config(const AlbaConfig& cfg)
{
    if (cfg.alba_connection_use_rora.value())
    {
        return apc::RoraConfig(cfg.alba_connection_rora_manifest_cache_capacity.value());
    }
    else
    {
        return boost::none;
    }
}

}

namespace albaconn
{

Connection::Connection(const config_type& cfg)
    : Connection(cfg.alba_connection_host.value(),
                 cfg.alba_connection_port.value(),
                 cfg.alba_connection_timeout.value(),
                 extract_preset(cfg),
                 boost::lexical_cast<apc::Transport>(cfg.alba_connection_transport.value()),
                 extract_rora_config(cfg))
{}

Connection::Connection(const string& host,
                       const uint16_t port,
                       const uint16_t timeout,
                       const boost::optional<std::string>& preset,
                       const apc::Transport transport,
                       const boost::optional<apc::RoraConfig>& rora_config)
    : preset_(preset)
    , healthy_(true)
{
    static bool initialized = false;
    static mutex initializer_lock;
    static function<void(al::AlbaLogLevel, string &)> logMessageFunction = logMessage;

    {
        lock_guard<decltype(initializer_lock)> g(initializer_lock);
        if (!initialized)
        {
            LOG_INFO("Setting up Alba log handler");

            al::setLogFunction([&](al::AlbaLogLevel albaLevel) -> function<void(al::AlbaLogLevel, string &)>*
                               {
                                   yt::Logger::logger_type alba_logger("AlbaProxyClient");
                                   auto level = convertAlbaLevel(albaLevel);
                                   if (yt::Logger::filter(alba_logger.name,
                                                          level))
                                   {
                                       return &logMessageFunction;
                                   }
                                   else
                                   {
                                       return nullptr;
                                   }
                               });
            initialized = true;
        }
    }

    client_ = apc::make_proxy_client(host,
                                     boost::lexical_cast<string>(port),
                                     std::chrono::seconds(timeout),
                                     transport,
                                     rora_config);
}

TODO("Y42: Better would be to specify the exceptions for each call")
template<typename R, typename F>
R
Connection::convert_exceptions_(const char* desc,
                                F&& fun)
{
    ASSERT(healthy_);

    try
    {
        return fun();
    }
    catch (apc::proxy_exception& e)
    {
        LOG_ERROR(desc << ": caught Alba proxy exception: " << e.what());

#define X(albaex, vdex)                         \
        case app::return_code::albaex:          \
            throw vdex()

        switch (e._return_code)
        {
            X(OVERWRITE_NOT_ALLOWED, BackendOverwriteNotAllowedException);
            X(OBJECT_DOES_NOT_EXIST, BackendObjectDoesNotExistException);
            X(NAMESPACE_ALREADY_EXISTS, BackendNamespaceAlreadyExistsException);
            X(NAMESPACE_DOES_NOT_EXIST, BackendNamespaceDoesNotExistException);
            X(CHECKSUM_MISMATCH, BackendInputException);
        default:
            healthy_ = false;
            throw;
        }

#undef X
    }
    catch (alba::llio::output_stream_exception& e)
    {
        LOG_ERROR(desc << ": caught ALBA output stream exception: " << e.what() <<
                  " - assuming we failed to connect to the proxy");
        healthy_ = false;
        throw BackendConnectFailureException();
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR(desc << ": caught exception: " << EWHAT);
            healthy_ = false;
            throw;
        });
}

bool
Connection::namespaceExists_(const Namespace& nspace)
{
    return convert_exceptions_<bool>("namespace exists",
                                     [&]() -> bool
        {
               return client_->namespace_exists(nspace.str());
        });
}

yt::CheckSum
Connection::getCheckSum_(const Namespace& nspace,
                         const string& name)
{
    return convert_exceptions_<yt::CheckSum>("get checksum",
                                             [&]() -> yt::CheckSum
        {
            TODO("This seems not to be the correct interface");

            alba::Checksum* c;
            std::tie(std::ignore, c) =
                client_->get_object_info(nspace.str(),
                                         name,
                                         apc::consistent_read::T,
                                         apc::should_cache::F);

            if (c)
            {
                std::unique_ptr<alba::Checksum> chksum(c);
                auto crc32c = dynamic_cast<const alba::Crc32c*>(chksum.get());
                if (crc32c)
                {
                    return yt::CheckSum(crc32c->_digest);
                }
                else
                {
                    LOG_ERROR(nspace << ": checksum of " << name << " is not a CRC32C");
                    throw BackendException("Returned checksum of object is not a CRC32C");
                }
            }
            else
            {
                LOG_ERROR(nspace << ": no checksum for " << name);
                throw BackendException("No checksum for object");
            }
        });
}

void
Connection::createNamespace_(const Namespace& nspace,
                             const NamespaceMustNotExist must_not_exist)
{
    try
    {
        convert_exceptions_<void>("create namespace",
                                  [&]
        {
            client_->create_namespace(nspace.str(),
                                      preset_);
        });
    }
    catch (BackendNamespaceAlreadyExistsException&)
    {
        if (must_not_exist == NamespaceMustNotExist::T)
        {
            throw BackendCouldNotCreateNamespaceException();
        }
    }
}

void
Connection::read_(const Namespace& nspace,
                  const fs::path& dst,
                  const string& name,
                  InsistOnLatestVersion insist_on_latest_version)
{
    LOG_TRACE(nspace << ": retrieving " << name << " to " <<
              dst);

    convert_exceptions_<void>("read object",
                              [&]
        {
            client_->read_object_fs(nspace.str(),
                                    name,
                                    dst.string(),
                                    T(insist_on_latest_version) ?
                                    alba::proxy_client::consistent_read::T :
                                    alba::proxy_client::consistent_read::F,
                                    alba::proxy_client::should_cache::F);
        });
}

bool
Connection::partial_read_(const Namespace& ns,
                          const PartialReads& partial_reads,
                          InsistOnLatestVersion insist_on_latest_version)
{
    if(partial_reads.empty())
    {
        return true;
    }

    std::vector<app::ObjectSlices> app_slicesv;
    app_slicesv.reserve(partial_reads.size());

    for (const auto& partial_read : partial_reads)
    {
        const ObjectSlices& slices = partial_read.second;
        VERIFY(not slices.empty());

        std::vector<app::SliceDescriptor> app_descs;
        app_descs.reserve(slices.size());

        uint64_t off = 0;

        for (const auto& s : slices)
        {
            VERIFY(s.size != 0);
            VERIFY(off <= s.offset);
            off = s.offset + s.size;

            app_descs.emplace_back(app::SliceDescriptor{ s.buf,
                                                         s.offset,
                                                         s.size });
        }

        app_slicesv.emplace_back(app::ObjectSlices { partial_read.first,
                                                     std::move(app_descs) });
    };

    convert_exceptions_<void>("partial read",
                              [&]
        {
            client_->read_objects_slices(ns.str(),
                                         app_slicesv,
                                         insist_on_latest_version == InsistOnLatestVersion::T ?
                                         alba::proxy_client::consistent_read::T :
                                         alba::proxy_client::consistent_read::F);
        });

    return true;
}


void
Connection::write_(const Namespace& nspace,
                   const fs::path &src,
                   const string& name,
                   const OverwriteObject overwrite,
                   const youtils::CheckSum* chksum)
{
    LOG_TRACE(nspace << ": storing " << src << " as " << name << ", overwrite: " <<
              overwrite << ", checksum " << chksum);

    convert_exceptions_<void>("write object",
                              [&]
        {
            const alba::Crc32c crc32c(chksum ? chksum->getValue() : 0);
            client_->write_object_fs(nspace.str(),
                                     name,
                                     src.string(),
                                     apc::allow_overwrite(overwrite == OverwriteObject::T),
                                     chksum ? &crc32c : nullptr);
        });
}

bool
Connection::objectExists_(const Namespace& nspace,
                          const string& name)
{
    LOG_TRACE(nspace << ": name");

    return convert_exceptions_<bool>("object exists",
                                     [&]() -> bool
        {
            try
            {
                uint64_t size;
                alba::Checksum* c;
                std::tie(size, c) = client_->get_object_info(nspace.str(),
                                                             name,
                                                             alba::proxy_client::consistent_read::T,
                                                             alba::proxy_client::should_cache::F);
                std::unique_ptr<alba::Checksum> chksum(c);
                return true;
            }
            catch (apc::proxy_exception& e)
            {
                if (e._return_code == app::return_code::OBJECT_DOES_NOT_EXIST)
                {
                    return false;
                }
                else
                {
                    throw;
                }
            }
        });
}

void
Connection::remove_(const Namespace& nspace,
                    const string& name,
                    const ObjectMayNotExist may_not_exist)
{
    LOG_TRACE(nspace << ": deleting " << name << ", may not exist: " << may_not_exist);

    convert_exceptions_<void>("delete object",
                              [&]
        {
            client_->delete_object(nspace.str(),
                                   name,
                                   apc::may_not_exist(may_not_exist == ObjectMayNotExist::T));
        });
}

void
Connection::deleteNamespace_(const Namespace& nspace)
try
{
    LOG_TRACE(nspace << ": deleting");

    convert_exceptions_<void>("delete namespace",
                              [&]
        {
            client_->delete_namespace(nspace.str());
        });
}
catch (BackendNamespaceDoesNotExistException&)
{
    throw BackendCouldNotDeleteNamespaceException();
}

void
Connection::invalidate_cache_(const boost::optional<Namespace>& nspace)
{
    LOG_INFO(nspace << ": requesting cache invalidation");

    if (not nspace)
    {
        LOG_ERROR("Invalidating cache for all namespaces is not supported");
        throw BackendNotImplementedException();
    }

    convert_exceptions_<void>("invalidate cache",
                              [&]
        {
            client_->invalidate_cache(nspace->str());
        });
}

uint64_t
Connection::getSize_(const Namespace& nspace,
                     const string& name)
{
    return convert_exceptions_<uint64_t>("get object size",
                                         [&]() -> uint64_t
        {
            uint64_t size;
            alba::Checksum* c;

            std::tie(size, c) = client_->get_object_info(nspace.str(),
                                                         name,
                                                         alba::proxy_client::consistent_read::T,
                                                         alba::proxy_client::should_cache::F);
            std::unique_ptr<alba::Checksum> chksum(c);
            return size;
        });
}

void
Connection::listObjects_(const Namespace& nspace,
                         list<string>& out)
{
    LOG_INFO("calling proxy_client::list_objects(" << nspace << ")");

    convert_exceptions_<void>("list objects",
                              [&]
        {
            auto include_first = apc::include_first::T;
            std::string first = "";
            auto has_more = apc::has_more::T;
            while (has_more == apc::has_more::T)
            {
                auto result = client_->list_objects(nspace.str(),
                                                    first,
                                                    include_first,
                                                    boost::none,
                                                    apc::include_last::T,
                                                    -1,
                                                    apc::reverse::F);

                auto objects = get<0>(result);
                has_more = get<1>(result);

                for (auto it = objects.begin(); it != objects.end(); ++it)
                {
                    out.push_back(*it);
                    first = *it;
                }
                if (has_more == apc::has_more::T)
                {
                    LOG_TRACE("There's more, calling proxy_client::list_objects(" <<
                             nspace << ")");
                    include_first = apc::include_first::F;
                }
            }
        });
}

void
Connection::listNamespaces_(list<string>& nspaces)
{
    convert_exceptions_<void>("list namespaces",
                              [&]
        {
            auto include_first = apc::include_first::T;
            std::string first = "";
            auto has_more = apc::has_more::T;
            while (has_more == apc::has_more::T)
            {
                auto result = client_->list_namespaces(first,
                                                       include_first,
                                                       boost::none,
                                                       apc::include_last::T,
                                                       -1,
                                                       apc::reverse::F);
                auto objects = get<0>(result);
                has_more = get<1>(result);
                for (auto it = objects.begin(); it != objects.end(); ++it)
                {
                    nspaces.push_back(*it);
                    first = *it;
                }
                if (has_more == apc::has_more::T)
                {
                    LOG_INFO("There's more, calling proxy_client::list_namespaces()");
                    include_first = apc::include_first::F;
                }
            }
        });
}

std::unique_ptr<yt::UniqueObjectTag>
Connection::get_tag_(const Namespace&,
                     const std::string&)
{
    LOG_ERROR("yt::UniqueObjectTag support is not available yet for Alba backend");
    throw BackendNotImplementedException();
}

std::unique_ptr<yt::UniqueObjectTag>
Connection::write_tag_(const Namespace&,
                       const fs::path&,
                       const std::string&,
                       const yt::UniqueObjectTag*,
                       const OverwriteObject)
{
    LOG_ERROR("yt::UniqueObjectTag support is not available yet for Alba backend");
    throw BackendNotImplementedException();
}

}

}
