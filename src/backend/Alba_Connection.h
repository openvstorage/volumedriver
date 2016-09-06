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

#ifndef ALBA_CONNECTION_H
#define ALBA_CONNECTION_H

#include "AlbaConfig.h"
#include "BackendConnectionInterface.h"

#include <alba/proxy_client.h>

namespace backend
{

namespace albaconn
{

class Connection
    : public BackendConnectionInterface
{
public:
    typedef AlbaConfig config_type;

    explicit Connection(const config_type& cfg);

    Connection(const std::string& host,
               const uint16_t port,
               const uint16_t timeout,
               const boost::optional<std::string>& preset = boost::none,
               const alba::proxy_client::Transport = alba::proxy_client::Transport::tcp,
               const boost::optional<alba::proxy_client::RoraConfig>& = boost::none);

    virtual ~Connection() noexcept (true)
    {}

    Connection(const Connection&) = delete;

    Connection&
    operator=(const Connection&) = delete;

private:
    virtual bool
    healthy() const override final
    {
        return healthy_;
    }

    virtual void
    listNamespaces_(std::list<std::string>& objects) override final;

    virtual void
    createNamespace_(const Namespace& nspace,
                     const NamespaceMustNotExist = NamespaceMustNotExist::T) override final ;

    virtual void
    deleteNamespace_(const Namespace& nspace) override final;

    virtual bool
    namespaceExists_(const Namespace& nspace) override final;

    virtual void
    invalidate_cache_(const boost::optional<Namespace>& nspace) override final;

    virtual bool
    objectExists_(const Namespace& nspace,
                  const std::string& name) override final;

    virtual uint64_t
    getSize_(const Namespace& nspace,
             const std::string& name) override final;

    virtual youtils::CheckSum
    getCheckSum_(const Namespace& nspace,
                 const std::string& name) override final;

    virtual void
    remove_(const Namespace& nspace,
            const std::string& name,
            const ObjectMayNotExist) override final;

    virtual void
    listObjects_(const Namespace& nspace,
                 std::list<std::string>& objects) override final;

    virtual void
    read_(const Namespace& nspace,
          const boost::filesystem::path& destination,
          const std::string& name,
          InsistOnLatestVersion) override final;

    virtual std::unique_ptr<youtils::UniqueObjectTag>
    get_tag_(const Namespace&,
             const std::string&) override final;

    virtual bool
    partial_read_(const Namespace& ns,
                  const PartialReads& partial_reads,
                  InsistOnLatestVersion) override final;

    virtual void
    write_(const Namespace& nspace,
           const boost::filesystem::path &location,
           const std::string& name,
           OverwriteObject,
           const youtils::CheckSum* chksum = 0) override final;

    virtual std::unique_ptr<youtils::UniqueObjectTag>
    write_tag_(const Namespace&,
               const boost::filesystem::path&,
               const std::string&,
               const youtils::UniqueObjectTag*,
               const OverwriteObject) override final;

    virtual bool
    hasExtendedApi_() const override final
    {
        return false;
    }

private:
    DECLARE_LOGGER("AlbaConnection");

    const boost::optional<std::string> preset_;
    std::unique_ptr<alba::proxy_client::Proxy_client> client_;
    bool healthy_;

    template<typename R, typename F>
    R
    convert_exceptions_(const char* desc,
                        F&& fun);
};

}

}

#endif // ALBA_CONNECTION_H
