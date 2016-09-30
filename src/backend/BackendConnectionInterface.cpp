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

#include "BackendConnectionInterface.h"

#include <boost/optional/optional_io.hpp>

#include <youtils/FileDescriptor.h>
#include <youtils/FileUtils.h>
#include <youtils/ScopeExit.h>

namespace backend
{

using namespace youtils;
using namespace std::literals::string_literals;

namespace
{

struct Logger
{
    Logger(const char* function,
           const Namespace& nspace,
           const std::string& object)
        : function_(function)
        , nspace_(nspace)
        , object_(object)
    {
        LOG_INFO("Entering " << function << " " << nspace << " " << object);
    }

    Logger(const char* function,
           const Namespace& nspace)
        : function_(function)
        , nspace_(nspace)
    {
        LOG_INFO("Entering " << function << " " << nspace);
    }

    Logger(const char* function,
           const boost::optional<Namespace>& nspace)
        : function_(function)
        , nspace_(nspace)
    {
        LOG_INFO("Entering " << function << " " << nspace);
    }

    explicit Logger(const char* function)
        : function_(function)
    {
        LOG_INFO("Entering " << function) ;
    }

    ~Logger()
    {
        const bool exception_pending = std::uncaught_exception();
        if (exception_pending)
        {
            if(nspace_ and object_)
            {
                LOG_ERROR("Exiting " << function_
                          << " for " << *nspace_
                          << " " << *object_ << " with exception");
            }
            if(nspace_)
            {
                LOG_ERROR("Exiting " << function_
                          << " for " << *nspace_ );
            }
            else
            {
                ASSERT(not object_);
                LOG_ERROR("Exiting " << function_);
            }
        }
        else
        {
            if(nspace_ and object_)
            {
                LOG_INFO("Exiting " << function_
                         << " for " << *nspace_
                         << " " << *object_);
            }
            if(nspace_)
            {
                LOG_INFO("Exiting " << function_
                         << " for " << *nspace_ );
            }
            else
            {
                ASSERT( not object_);
                LOG_INFO("Exiting " << function_);
            }
        }
    }

    DECLARE_LOGGER("BackendConnectionInterfaceLogger");

    const char* function_;
    boost::optional<const Namespace&> nspace_;
    boost::optional<const std::string&> object_;
};

}

void
BackendConnectionInterface::listNamespaces(std::list<std::string>& nspaces)
{
    Logger l(__FUNCTION__);
    return listNamespaces_(nspaces);
}

void
BackendConnectionInterface::listObjects(const Namespace& nspace,
                                        std::list<std::string>& objects)
{
    Logger l(__FUNCTION__, nspace);
    return listObjects_(nspace, objects);
}

bool
BackendConnectionInterface::namespaceExists(const Namespace& nspace)
{
    Logger l(__FUNCTION__, nspace);
    return namespaceExists_(nspace);
}

void
BackendConnectionInterface::createNamespace(const Namespace& nspace,
                                            const NamespaceMustNotExist must_not_exist)
{
    Logger l(__FUNCTION__, nspace);
    return createNamespace_(nspace,
                            must_not_exist);
}

void
BackendConnectionInterface::deleteNamespace(const Namespace& nspace)
{
    Logger l(__FUNCTION__, nspace);
    return deleteNamespace_(nspace);
}

void
BackendConnectionInterface::clearNamespace(const Namespace& nspace)
{
    Logger l(__FUNCTION__, nspace);
    return clearNamespace_(nspace);
}

void
BackendConnectionInterface::invalidate_cache(const boost::optional<Namespace>& nspace)
{
    Logger l(__FUNCTION__, nspace);
    return invalidate_cache_(nspace);
}

void
BackendConnectionInterface::read(const Namespace& nspace,
                                 const boost::filesystem::path& destination,
                                 const std::string& name,
                                 const InsistOnLatestVersion insist_on_latest)
{
    Logger l(__FUNCTION__, nspace, name);

    //cfr SSOBF-9585
    //The target file is synced for the following reason:
    //Suppose a scocachemiss reads SCO X from the backend, then there is
    //a powerloss, the volume is locally restarted and reuses the local scocache.
    //Although the scocache assumes it has SCO X cached, it could very well contain no
    //data at all as it got lost during the powerloss. This then results in a
    // read-error from that SCO, leading to offlining a mountpoint (for no good
    // reason) and (when the last mountpoint is gone) halting volumes.
    boost::filesystem::path temp_file;
    try
    {
        temp_file = youtils::FileUtils::create_temp_file(destination);
    }
    catch(...)
    {
        throw BackendOutputException();
    }

    ALWAYS_CLEANUP_FILE(temp_file);
    read_(nspace, temp_file, name, insist_on_latest);
    youtils::FileUtils::syncAndRename(temp_file, destination);
}

std::unique_ptr<UniqueObjectTag>
BackendConnectionInterface::get_tag(const Namespace& nspace,
                                    const std::string& name)
{
    Logger l(__FUNCTION__,
             nspace,
             name);

    return get_tag_(nspace,
                     name);
}

void
BackendConnectionInterface::partial_read(const Namespace& ns,
                                         const PartialReads& partial_reads,
                                         InsistOnLatestVersion insist_on_latest,
                                         PartialReadFallbackFun& fallback_fun)
try
{
    const bool ok = partial_read_(ns,
                                  partial_reads,
                                  insist_on_latest);
    if (not ok)
    {
        LOG_TRACE(ns << ": partial read not supported - falling back to emulation");

        for(const auto& p : partial_reads)
        {
            youtils::FileDescriptor& sio = fallback_fun(ns,
                                                        p.first,
                                                        insist_on_latest);

            for (const auto& s : p.second)
            {
                const size_t res = sio.pread(s.buf,
                                             s.size,
                                             s.offset);
                if(res != s.size)
                {
                    LOG_ERROR(ns << "/" << p.first <<
                              ": read less (" << res <<
                              ") than expected (" << s.size << ") from offset " <<
                              s.offset);
                    throw BackendRestoreException();
                }
            }
        }
    }
}
CATCH_STD_ALL_EWHAT({
        LOG_ERROR(ns << ": partial read failed: " << EWHAT);
        throw; // redundant
    })

void
BackendConnectionInterface::write(const Namespace& nspace,
                                  const boost::filesystem::path& location,
                                  const std::string& name,
                                  const OverwriteObject overwrite,
                                  const youtils::CheckSum* chksum,
                                  const boost::shared_ptr<Condition>& cond)
{
    Logger l(__FUNCTION__, nspace, name);
    return write_(nspace, location, name, overwrite, chksum, cond);
}

std::unique_ptr<UniqueObjectTag>
BackendConnectionInterface::write_tag(const Namespace& nspace,
                                      const fs::path& src,
                                      const std::string& name,
                                      const UniqueObjectTag* prev_tag,
                                      const OverwriteObject overwrite)
{
    Logger l(__FUNCTION__, nspace, name);

    VERIFY(not (prev_tag and overwrite == OverwriteObject::F));

    return write_tag_(nspace,
                      src,
                      name,
                      prev_tag,
                      overwrite);
}

std::unique_ptr<UniqueObjectTag>
BackendConnectionInterface::read_tag(const Namespace& nspace,
                                     const fs::path& src,
                                     const std::string& name)
{
    Logger l(__FUNCTION__, nspace, name);

    return read_tag_(nspace,
                     src,
                     name);
}

bool
BackendConnectionInterface::objectExists(const Namespace& nspace,
                                         const std::string& name)
{
    Logger l(__FUNCTION__, nspace, name);
    return objectExists_(nspace, name);
}

void
BackendConnectionInterface::remove(const Namespace& nspace,
                                   const std::string& name,
                                   const ObjectMayNotExist may_not_exist,
                                   const boost::shared_ptr<Condition>& cond)
{
    Logger l(__FUNCTION__, nspace, name);
    return remove_(nspace,
                   name,
                   may_not_exist,
                   cond);
}

uint64_t
BackendConnectionInterface::getSize(const Namespace& nspace,
                                    const std::string& name)
{
    Logger l(__FUNCTION__, nspace, name);
    return getSize_(nspace, name);
}

youtils::CheckSum
BackendConnectionInterface::getCheckSum(const Namespace& nspace,
                                        const std::string& name)
{
    Logger l(__FUNCTION__, nspace, name);
    return getCheckSum_(nspace, name);
}

void
BackendConnectionInterface::invalidate_cache_(const boost::optional<Namespace>& nspace)
{
    LOG_INFO(nspace <<
             ": not invalidating local cache because there presumably isn't one");
}

void
BackendConnectionInterface::clearNamespace_(const Namespace& nspace)
{
    deleteNamespace_(nspace);
    invalidate_cache_(nspace);
    return createNamespace_(nspace);

}

}
