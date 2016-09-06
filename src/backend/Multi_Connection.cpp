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

#include "Multi_Connection.h"
#include "Local_Connection.h"

namespace backend
{
namespace multi
{

namespace yt = youtils;

Connection::Connection(const config_type& cfg)
{
    const std::vector<std::unique_ptr<BackendConfig> >& configs = cfg.configs_;

    for(size_t i = 0; i < configs.size(); ++i)
    {
        const BackendConfig* config = configs[i].get();
        switch (config->backend_type.value())
        {
        case BackendType::LOCAL:
            {
                const LocalConfig* config_tmp(dynamic_cast<const LocalConfig*>(config));
                backends_.push_back(new local::Connection(*config_tmp));
                break;

            }
        default:
            throw "backend type not supported in MULTI backend";
        }
    }
    current_iterator_ = backends_.begin();

}

Connection::~Connection()
{
    for(unsigned i = 0; i < backends_.size(); ++i)
    {
        delete backends_[i];
    }
}

bool
Connection::hasExtendedApi_() const
{
    if(current_iterator_ != backends_.end())
    {
        return (*current_iterator_)->hasExtendedApi_();
    }
    throw BackendNoMultiBackendAvailableException();
}

bool
Connection::namespaceExists_(const Namespace& nspace)
{
    iterator_t start_iterator = current_iterator_;

    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->namespaceExists(nspace);
        }
        catch(std::exception& e)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

yt::CheckSum
Connection::getCheckSum_(const Namespace& nspace,
                        const std::string& name)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->getCheckSum(nspace,
                                                     name);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }

    }
    throw BackendNoMultiBackendAvailableException();
}

void
Connection::createNamespace_(const Namespace& nspace,
                             const NamespaceMustNotExist must_not_exist)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->createNamespace(nspace,
                                                         must_not_exist);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendNamespaceAlreadyExistsException&)
        {
            throw;
        }

        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

void
Connection::read_(const Namespace& nspace,
                  const fs::path& dst,
                  const std::string& name,
                  InsistOnLatestVersion insist_on_latest_version)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->read(nspace,
                                              dst,
                                              name,
                                              insist_on_latest_version);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendNamespaceAlreadyExistsException&)
        {
            throw;
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

bool
Connection::partial_read_(const Namespace& nspace,
                          const PartialReads& partial_reads,
                          InsistOnLatestVersion insist)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->partial_read_(nspace,
                                                       partial_reads,
                                                       insist);
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

void
Connection::write_(const Namespace& nspace,
                   const fs::path& src,
                   const std::string& name,
                   OverwriteObject overwrite,
                   const yt::CheckSum* chksum)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->write(nspace,
                                               src,
                                               name,
                                               overwrite,
                                               chksum);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendNamespaceAlreadyExistsException&)
        {
            throw;
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

bool
Connection::objectExists_(const Namespace& nspace,
                         const std::string& name)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->objectExists(nspace,
                                                      name);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendNamespaceAlreadyExistsException&)
        {
            throw;
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

void
Connection::remove_(const Namespace& nspace,
                    const std::string& name,
                    const ObjectMayNotExist may_not_exist)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->remove(nspace,
                                                name,
                                                may_not_exist);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendNamespaceAlreadyExistsException&)
        {
            throw;
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

void
Connection::deleteNamespace_(const Namespace& nspace)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->deleteNamespace(nspace);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendNamespaceAlreadyExistsException&)
        {
            throw;
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

uint64_t
Connection::getSize_(const Namespace& nspace,
                    const std::string& name)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->getSize(nspace,
                                                 name);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendNamespaceAlreadyExistsException&)
        {
            throw;
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

void
Connection::listNamespaces_(std::list<std::string>& nspaces)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->listNamespaces(nspaces);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendNamespaceAlreadyExistsException&)
        {
            throw;
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}



void
Connection::listObjects_(const Namespace& nspace,
                        std::list<std::string>& out)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->listObjects(nspace,
                                                     out);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendNamespaceAlreadyExistsException&)
        {
            throw;
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

backend::ObjectInfo
Connection::x_getMetadata_(const Namespace& nspace,
                           const std::string& name)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->x_getMetadata(nspace,
                                                       name);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

backend::ObjectInfo
Connection::x_setMetadata_(const Namespace& nspace,
                          const std::string& name,
                          const backend::ObjectInfo::CustomMetaData& metadata)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->x_setMetadata(nspace,
                                                       name,
                                                       metadata);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

backend::ObjectInfo
Connection::x_updateMetadata_(const Namespace& nspace,
                             const std::string& name,
                             const backend::ObjectInfo::CustomMetaData& metadata)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->x_updateMetadata(nspace,
                                                       name,
                                                       metadata);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}



backend::ObjectInfo
Connection::x_read_(const Namespace& nspace,
                    const fs::path& destination,
                    const std::string& name,
                    const InsistOnLatestVersion insist_on_latest_version)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->x_read(nspace,
                                                destination,
                                                name,
                                                insist_on_latest_version);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();

}

backend::ObjectInfo
Connection::x_read_(const Namespace& nspace,
                    std::string& destination,
                    const std::string& name,
                    const InsistOnLatestVersion insist_on_latest_version)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->x_read(nspace,
                                                destination,
                                                name,
                                                insist_on_latest_version);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

backend::ObjectInfo
Connection::x_read_(const Namespace& nspace,
                    std::stringstream& destination,
                    const std::string& name,
                    const InsistOnLatestVersion insist_on_latest_version)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->x_read(nspace,
                                                destination,
                                                name,
                                                insist_on_latest_version);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

backend::ObjectInfo
Connection::x_write_(const Namespace& nspace,
                    const fs::path& location,
                    const std::string& name,
                    OverwriteObject overwrite,
                    const backend::ETag* etag,
                    const yt::CheckSum* chksum)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->x_write(nspace,
                                                 location,
                                                 name,
                                                 overwrite,
                                                 etag,
                                                 chksum);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

backend::ObjectInfo
Connection::x_write_(const Namespace& nspace,
                     const std::string& location,
                     const std::string& name,
                     OverwriteObject overwrite,
                     const backend::ETag* etag,
                     const yt::CheckSum* chksum)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->x_write(nspace,
                                                 location,
                                                 name,
                                                 overwrite,
                                                 etag,
                                                 chksum);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

backend::ObjectInfo
Connection::x_write_(const Namespace& nspace,
                     std::stringstream& location,
                     const std::string& name,
                     OverwriteObject overwrite,
                     const backend::ETag* etag,
                     const yt::CheckSum* chksum)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->x_write(nspace,
                                                 location,
                                                 name,
                                                 overwrite,
                                                 etag,
                                                 chksum);
        }
        catch(BackendNotImplementedException)
        {
            throw;
        }
        catch(BackendObjectDoesNotExistException)
        {
            throw;
        }
        catch(std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

std::unique_ptr<yt::UniqueObjectTag>
Connection::get_tag_(const Namespace& nspace,
                     const std::string& name)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->get_tag(nspace,
                                                 name);
        }
        catch (BackendNotImplementedException&)
        {
            throw;
        }
        catch (BackendObjectDoesNotExistException&)
        {
            throw;
        }
        catch (std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

std::unique_ptr<yt::UniqueObjectTag>
Connection::write_tag_(const Namespace& nspace,
                       const fs::path& src,
                       const std::string& name,
                       const yt::UniqueObjectTag* prev_tag,
                       const OverwriteObject overwrite)
{
    iterator_t start_iterator = current_iterator_;
    while(maybe_switch_back_to_default())
    {
        try
        {
            return (*current_iterator_)->write_tag(nspace,
                                                   src,
                                                   name,
                                                   prev_tag,
                                                   overwrite);
        }
        catch (BackendUniqueObjectTagMismatchException&)
        {
            throw;
        }
        catch (BackendNotImplementedException&)
        {
            throw;
        }
        catch (BackendObjectDoesNotExistException&)
        {
            throw;
        }
        catch (std::exception&)
        {
            if(update_current_index(start_iterator))
            {
                throw;
            }
        }
    }
    throw BackendNoMultiBackendAvailableException();
}

}
}

// Local Variables: **
// mode: c++ **
// End: **
