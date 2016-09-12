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

    backends_.reserve(configs.size());

    for (const auto& config : configs)
    {
        switch (config->backend_type.value())
        {
        case BackendType::LOCAL:
            {
                const LocalConfig* config_tmp(dynamic_cast<const LocalConfig*>(config.get()));
                backends_.emplace_back(std::make_unique<local::Connection>(*config_tmp));
                break;
            }
        default:
            throw "backend type not supported in MULTI backend";
        }
    }

    current_iterator_ = backends_.begin();
}

template<typename Ret,
         typename... Args>
Ret
Connection::wrap_(Ret (BackendConnectionInterface::*fun)(Args...),
                  Args... args)
{
    ConnVector::iterator start_iterator = current_iterator_;

    while (maybe_switch_back_to_default())
    {
        try
        {
            BackendConnectionInterface& conn = *current_iterator_->get();
            return (conn.*fun)(std::forward<Args>(args)...);
        }
        catch (BackendNamespaceAlreadyExistsException&)
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
        catch (BackendUniqueObjectTagMismatchException&)
        {
            throw;
        }
        catch (std::exception&)
        {
            if (update_current_index(start_iterator))
            {
                throw;
            }
        }
    }

    throw BackendNoMultiBackendAvailableException();
}

bool
Connection::namespaceExists_(const Namespace& nspace)
{
    return wrap_<bool,
                 decltype(nspace)>(&BackendConnectionInterface::namespaceExists,
                                   nspace);
}

yt::CheckSum
Connection::getCheckSum_(const Namespace& nspace,
                        const std::string& name)
{
    return wrap_<yt::CheckSum,
                 decltype(nspace),
                 decltype(name)>(&BackendConnectionInterface::getCheckSum,
                                 nspace,
                                 name);
}

void
Connection::createNamespace_(const Namespace& nspace,
                             const NamespaceMustNotExist must_not_exist)
{
    return wrap_<void,
                 decltype(nspace),
                 NamespaceMustNotExist>(&BackendConnectionInterface::createNamespace,
                                        nspace,
                                        must_not_exist);
}

void
Connection::read_(const Namespace& nspace,
                  const fs::path& dst,
                  const std::string& name,
                  InsistOnLatestVersion insist_on_latest_version)
{
    return wrap_<void,
                 decltype(nspace),
                 decltype(dst),
                 decltype(name),
                 decltype(insist_on_latest_version)>(&BackendConnectionInterface::read,
                                                     nspace,
                                                     dst,
                                                     name,
                                                     insist_on_latest_version);
}

bool
Connection::partial_read_(const Namespace& nspace,
                          const PartialReads& partial_reads,
                          InsistOnLatestVersion insist)
{
    return wrap_<bool,
                 decltype(nspace),
                 decltype(partial_reads),
                 decltype(insist)>(&BackendConnectionInterface::partial_read_,
                                   nspace,
                                   partial_reads,
                                   insist);
}

void
Connection::write_(const Namespace& nspace,
                   const fs::path& src,
                   const std::string& name,
                   OverwriteObject overwrite,
                   const youtils::CheckSum* chksum,
                   const boost::shared_ptr<Condition>& cond)
{
    return wrap_<void,
                 decltype(nspace),
                 decltype(src),
                 decltype(name),
                 decltype(overwrite),
                 decltype(chksum),
                 decltype(cond)>(&BackendConnectionInterface::write,
                                 nspace,
                                 src,
                                 name,
                                 overwrite,
                                 chksum,
                                 cond);
}

bool
Connection::objectExists_(const Namespace& nspace,
                         const std::string& name)
{
    return wrap_<bool,
                 decltype(nspace),
                 decltype(name)>(&BackendConnectionInterface::objectExists,
                                 nspace,
                                 name);
}

void
Connection::remove_(const Namespace& nspace,
                    const std::string& name,
                    const ObjectMayNotExist may_not_exist,
                    const boost::shared_ptr<Condition>& cond)
{
    return wrap_<void,
                 decltype(nspace),
                 decltype(name),
                 ObjectMayNotExist,
                 decltype(cond)>(&BackendConnectionInterface::remove,
                                 nspace,
                                 name,
                                 may_not_exist,
                                 cond);
}

void
Connection::deleteNamespace_(const Namespace& nspace)
{
    return wrap_<void,
                 decltype(nspace)>(&BackendConnectionInterface::deleteNamespace,
                                   nspace);
}

uint64_t
Connection::getSize_(const Namespace& nspace,
                    const std::string& name)
{
    return wrap_<uint64_t,
                 decltype(nspace),
                 decltype(name)>(&BackendConnectionInterface::getSize,
                                 nspace,
                                 name);
}

void
Connection::listNamespaces_(std::list<std::string>& nspaces)
{
    return wrap_<void,
                 decltype(nspaces)>(&BackendConnectionInterface::listNamespaces,
                                    nspaces);
}

void
Connection::listObjects_(const Namespace& nspace,
                        std::list<std::string>& out)
{
    return wrap_<void,
                 decltype(nspace),
                 decltype(out)>(&BackendConnectionInterface::listObjects,
                                nspace,
                                out);
}

std::unique_ptr<yt::UniqueObjectTag>
Connection::get_tag_(const Namespace& nspace,
                     const std::string& name)
{
    return wrap_<std::unique_ptr<yt::UniqueObjectTag>,
                 decltype(nspace),
                 decltype(name)>(&BackendConnectionInterface::get_tag,
                                 nspace,
                                 name);
}

std::unique_ptr<yt::UniqueObjectTag>
Connection::write_tag_(const Namespace& nspace,
                       const fs::path& src,
                       const std::string& name,
                       const yt::UniqueObjectTag* prev_tag,
                       const OverwriteObject overwrite)
{
    return wrap_<std::unique_ptr<yt::UniqueObjectTag>,
                 decltype(nspace),
                 decltype(src),
                 decltype(name),
                 decltype(prev_tag),
                 OverwriteObject>(&BackendConnectionInterface::write_tag,
                                  nspace,
                                  src,
                                  name,
                                  prev_tag,
                                  overwrite);
}

std::unique_ptr<yt::UniqueObjectTag>
Connection::read_tag_(const Namespace& nspace,
                      const fs::path& dst,
                      const std::string& name)
{
    return wrap_<std::unique_ptr<yt::UniqueObjectTag>,
                 decltype(nspace),
                 decltype(dst),
                 decltype(name)>(&BackendConnectionInterface::read_tag,
                                 nspace,
                                 dst,
                                 name);
}

}

}

// Local Variables: **
// mode: c++ **
// End: **
