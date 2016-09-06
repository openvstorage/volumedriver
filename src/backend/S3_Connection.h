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

#ifndef S3_CONNECTION_H_
#define S3_CONNECTION_H_

#include "S3Config.h"
#include "BackendConnectionInterface.h"

#include <boost/filesystem.hpp>

 #include <webstor/wsconn.h>

namespace backend
{
typedef uint64_t S3Status;

namespace s3
{

class Connection
    : public BackendConnectionInterface
{
public:
    typedef S3Config config_type;

    explicit Connection(const config_type& cfg);

    Connection(S3Flavour flavour,
               const std::string host,
               const uint16_t port,
               const std::string& username,
               const std::string& password,
               bool verbose_logging,
               const UseSSL use_ssl,
               const SSLVerifyHost ssl_verify_host,
               const boost::filesystem::path& ssl_cert_file);

    virtual ~Connection() = default;

    Connection(const Connection&) = delete;

    Connection&
    operator=(const Connection&) = delete;

private
:
    bool
    healthy () const override
    {
        return true;
    }

    virtual void
    listNamespaces_(std::list<std::string>& objects) override final;

    virtual void
    createNamespace_(const Namespace& nspace,
                     const NamespaceMustNotExist = NamespaceMustNotExist::T) override final ;

    virtual void
    deleteNamespace_(const Namespace& nspace) override final;

    virtual void
    clearNamespace_(const Namespace& nspace) override final;

    virtual bool
    namespaceExists_(const Namespace& nspace) override final;

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
    partial_read_(const Namespace& /* ns */,
                  const PartialReads& /* partial_reads */,
                  InsistOnLatestVersion) override final
    {
        return false;
    }

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
    DECLARE_LOGGER("S3Connection");

    std::unique_ptr<webstor::WsConnection> ws_connection_;

    const S3Flavour flavour_;
    const std::string host_;
    const uint16_t port_;
    const std::string username_;
    const std::string password_;
    const std::string uri_style_;
    bool verbose_logging_;
    const UseSSL use_ssl_;
    const SSLVerifyHost ssl_verify_host_;
    const boost::filesystem::path ssl_cert_file_;

    static const unsigned max_retries = 5;
    static const unsigned retry_timout_in_milliseconds = 1000;

    std::unique_ptr<webstor::WsConnection>
    new_connection_() const;

    void setup_();
};

}
}

#endif // S3_CONNECTION_H_

// Local Variables: **
// mode: c++ **
// End: **
