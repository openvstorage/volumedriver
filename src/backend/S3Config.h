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

#ifndef BACKEND_S3_CONFIG_H_
#define BACKEND_S3_CONFIG_H_

#include "BackendConfig.h"

#include <youtils/Logging.h>

namespace backend
{
class S3Config
    : public BackendConfig
{
    friend class boost::serialization::access;

public:
    S3Config(S3Flavour flavour,
             const std::string& host,
             uint16_t port,
             const std::string& username,
             const std::string& password,
             const bool verbose_logging,
             const UseSSL use_ssl,
             const SSLVerifyHost ssl_verify_host,
             const boost::filesystem::path& ssl_cert_file,
             const StrictConsistency strict_consistency = StrictConsistency::F)
        :BackendConfig(BackendType::S3)
        , s3_connection_flavour(flavour)
        , s3_connection_host(host)
        , s3_connection_port(port)
        , s3_connection_username(username)
        , s3_connection_password(password)
        , s3_connection_verbose_logging(verbose_logging)
        , s3_connection_use_ssl(use_ssl == UseSSL::T)
        , s3_connection_ssl_verify_host(ssl_verify_host == SSLVerifyHost::T)
        , s3_connection_ssl_cert_file(ssl_cert_file.string())
        , s3_connection_strict_consistency(strict_consistency == StrictConsistency::T)
    {}

    S3Config(const boost::property_tree::ptree& pt)
        : BackendConfig(BackendType::S3)
        , s3_connection_flavour(pt)
        , s3_connection_host(pt)
        , s3_connection_port(pt)
        , s3_connection_username(pt)
        , s3_connection_password(pt)
        , s3_connection_verbose_logging(pt)
        , s3_connection_use_ssl(pt)
        , s3_connection_ssl_verify_host(pt)
        , s3_connection_ssl_cert_file(pt)
        , s3_connection_strict_consistency(pt)
    {}

    // This should also use the strings in BackendParameters
    virtual const char*
    name() const override final
    {
        static const char* n = "S3";
        return n;
    }

    S3Config() = delete;

    S3Config(const S3Config&) = delete;

    S3Config&
    operator=(const S3Config&) = delete;

    virtual StrictConsistency
    strict_consistency() const override final
    {
        return s3_connection_strict_consistency.value() ?
            StrictConsistency::T :
            StrictConsistency::F;
    }

    virtual bool
    unique_tag_support() const override final
    {
        return false;
    }

    virtual std::unique_ptr<BackendConfig>
    clone() const override final
    {
        std::unique_ptr<BackendConfig>
            bc(new S3Config(s3_connection_flavour.value(),
                            s3_connection_host.value(),
                            s3_connection_port.value(),
                            s3_connection_username.value(),
                            s3_connection_password.value(),
                            s3_connection_verbose_logging.value(),
                            s3_connection_use_ssl.value() ?
                            UseSSL::T :
                            UseSSL::F,
                            s3_connection_ssl_verify_host.value() ?
                            SSLVerifyHost::T :
                            SSLVerifyHost::F,
                            s3_connection_ssl_cert_file.value(),
                            s3_connection_strict_consistency.value() ?
                            StrictConsistency::T :
                            StrictConsistency::F));
        return bc;
    }

    virtual void
    update_internal(const boost::property_tree::ptree&,
                    youtils::UpdateReport&) override final
    {}

    virtual bool
    operator==(const BackendConfig& other) const override final
    {
#define EQ(x)                                                   \
        (static_cast<const S3Config&>(other).  x  .value() == x  .value())

        return other.backend_type.value() == BackendType::S3 and
            EQ(s3_connection_flavour) and
            EQ(s3_connection_host) and
            EQ(s3_connection_port) and
            EQ(s3_connection_username) and
            EQ(s3_connection_password) and
            EQ(s3_connection_verbose_logging) and
            EQ(s3_connection_use_ssl) and
            EQ(s3_connection_ssl_verify_host) and
            EQ(s3_connection_ssl_cert_file) and
            EQ(s3_connection_strict_consistency);

#undef EQ
    }

    virtual void
    persist_internal(boost::property_tree::ptree& pt,
                     const ReportDefault reportDefault) const override final
    {
        putBackendType(pt, reportDefault);

#define P(x)                                            \
        (x).persist(pt, reportDefault)

        P(s3_connection_flavour);
        P(s3_connection_host);
        P(s3_connection_port);
        P(s3_connection_username);
        P(s3_connection_password);
        P(s3_connection_verbose_logging);
        P(s3_connection_use_ssl);
        P(s3_connection_ssl_verify_host);
        P(s3_connection_ssl_cert_file);
        P(s3_connection_strict_consistency);

#undef P
    }

    virtual bool
    checkConfig_internal(const boost::property_tree::ptree&,
                         youtils::ConfigurationReport&) const override final
    {
        // Check whether the Uri Style is correct...
        return true;
    }

    DECLARE_PARAMETER(s3_connection_flavour);
    DECLARE_PARAMETER(s3_connection_host);
    DECLARE_PARAMETER(s3_connection_port);
    DECLARE_PARAMETER(s3_connection_username);
    DECLARE_PARAMETER(s3_connection_password);
    DECLARE_PARAMETER(s3_connection_verbose_logging);
    DECLARE_PARAMETER(s3_connection_use_ssl);
    DECLARE_PARAMETER(s3_connection_ssl_verify_host);
    DECLARE_PARAMETER(s3_connection_ssl_cert_file);
    DECLARE_PARAMETER(s3_connection_strict_consistency);
};

}

#endif // !BACKEND_S3_CONFIG_H_
