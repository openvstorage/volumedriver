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

#ifndef BACKEND_ALBA_CONFIG_H_
#define BACKEND_ALBA_CONFIG_H_

#include "BackendConfig.h"

#include <alba/proxy_client.h>

#include <youtils/Logging.h>

namespace backend
{

class AlbaConfig
    : public BackendConfig
{
public:
    AlbaConfig(const std::string& host,
               const uint16_t port,
               const uint16_t timeout,
               const alba::proxy_client::Transport transport = alba::proxy_client::Transport::tcp,
               const bool use_rora = false,
               const size_t rora_manifest_cache_capacity = 10000,
               const std::string& preset = "")

        : BackendConfig(BackendType::ALBA)
        , alba_connection_host(host)
        , alba_connection_port(port)
        , alba_connection_timeout(timeout)
        , alba_connection_preset(preset)
        , alba_connection_transport(transport)
        , alba_connection_use_rora(use_rora)
        , alba_connection_rora_manifest_cache_capacity(rora_manifest_cache_capacity)
    {}

    AlbaConfig(const boost::property_tree::ptree& pt)
        : BackendConfig(BackendType::ALBA)
        , alba_connection_host(pt)
        , alba_connection_port(pt)
        , alba_connection_timeout(pt)
        , alba_connection_preset(pt)
        , alba_connection_transport(pt)
        , alba_connection_use_rora(pt)
        , alba_connection_rora_manifest_cache_capacity(pt)
    {}

    AlbaConfig() = delete;

    AlbaConfig(const AlbaConfig&) = delete;

    AlbaConfig&
    operator=(const AlbaConfig&) = delete;

    virtual StrictConsistency
    strict_consistency() const override final
    {
        // Because proxy caching.
        return StrictConsistency::F;
    }

    virtual bool
    unique_tag_support() const override final
    {
        return false;
    }

    virtual const char*
    name() const override final
    {
        static const char* n = "ALBA";
        return n;
    }

    virtual std::unique_ptr<BackendConfig>
    clone() const override final
    {
        std::unique_ptr<BackendConfig>
            bc(new AlbaConfig(alba_connection_host.value(),
                              alba_connection_port.value(),
                              alba_connection_timeout.value(),
                              alba_connection_transport.value(),
                              alba_connection_use_rora.value(),
                              alba_connection_rora_manifest_cache_capacity.value(),
                              alba_connection_preset.value()));
        return bc;
    }

    virtual void
    persist_internal(boost::property_tree::ptree& pt,
                     const ReportDefault report_default) const override final
    {
        putBackendType(pt,
                       report_default);
        alba_connection_host.persist(pt,
                                     report_default);
        alba_connection_port.persist(pt,
                                     report_default);
        alba_connection_timeout.persist(pt,
                                        report_default);
        alba_connection_preset.persist(pt,
                                       report_default);
        alba_connection_transport.persist(pt,
                                          report_default);
        alba_connection_use_rora.persist(pt,
                                         report_default);
        alba_connection_rora_manifest_cache_capacity.persist(pt,
                                                             report_default);
    }

    virtual void
    update_internal(const boost::property_tree::ptree&,
                    youtils::UpdateReport&) override final
    {}

    virtual bool
    operator==(const BackendConfig& other) const override final
    {
#define CMP(x)                                                          \
        (static_cast<const AlbaConfig&>(other).x.value() == x.value())

        return
            other.backend_type.value() == BackendType::ALBA and
            CMP(alba_connection_host) and
            CMP(alba_connection_port) and
            CMP(alba_connection_timeout) and
            CMP(alba_connection_preset) and
            CMP(alba_connection_transport) and
            CMP(alba_connection_use_rora) and
            CMP(alba_connection_rora_manifest_cache_capacity);
#undef CMP
    }

    virtual bool
    checkConfig_internal(const boost::property_tree::ptree&,
                         youtils::ConfigurationReport&) const override final
    {
        return true;
    }

    DECLARE_PARAMETER(alba_connection_host);
    DECLARE_PARAMETER(alba_connection_port);
    DECLARE_PARAMETER(alba_connection_timeout);
    DECLARE_PARAMETER(alba_connection_preset);
    DECLARE_PARAMETER(alba_connection_transport);
    DECLARE_PARAMETER(alba_connection_use_rora);
    DECLARE_PARAMETER(alba_connection_rora_manifest_cache_capacity);
};

}

#endif // !BACKEND_ALBA_CONFIG_H_
