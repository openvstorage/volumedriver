// Copyright (C) 2017 iNuron NV
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

#ifndef BACKEND_CONNECTION_MANAGER_PARAMETERS_H_
#define BACKEND_CONNECTION_MANAGER_PARAMETERS_H_

#include <boost/property_tree/ptree_fwd.hpp>

#include <youtils/InitializedParam.h>
#include <youtils/UpdateReport.h>

namespace backend
{

struct ConnectionManagerParameters
{
    ConnectionManagerParameters() = default;

    explicit ConnectionManagerParameters(const boost::property_tree::ptree& pt)
        : backend_connection_pool_capacity(pt)
        , backend_connection_pool_blacklist_secs(pt)
        , backend_interface_retries_on_error(pt)
        , backend_interface_retry_interval_secs(pt)
        , backend_interface_retry_backoff_multiplier(pt)
        , backend_interface_partial_read_nullio(pt)
        , backend_interface_switch_connection_pool_policy(pt)
        , backend_interface_switch_connection_pool_partial_read_policy(pt)
        , backend_interface_switch_connection_pool_on_error_policy(pt)
    {}

    ~ConnectionManagerParameters() = default;

    ConnectionManagerParameters(const ConnectionManagerParameters&) = default;

    ConnectionManagerParameters&
    operator=(const ConnectionManagerParameters&) = default;

    void
    update(const boost::property_tree::ptree& pt,
           youtils::UpdateReport& rep)
    {
        backend_connection_pool_capacity.update(pt, rep);
        backend_connection_pool_blacklist_secs.update(pt, rep);
        backend_interface_retries_on_error.update(pt, rep);
        backend_interface_retry_interval_secs.update(pt, rep);
        backend_interface_retry_backoff_multiplier.update(pt, rep);
        backend_interface_partial_read_nullio.update(pt, rep);
        backend_interface_switch_connection_pool_policy.update(pt, rep);
        backend_interface_switch_connection_pool_partial_read_policy.update(pt, rep);
        backend_interface_switch_connection_pool_on_error_policy.update(pt, rep);
    }

    void
    persist(boost::property_tree::ptree& pt,
            const ReportDefault rep) const
    {
        backend_connection_pool_capacity.persist(pt, rep);
        backend_connection_pool_blacklist_secs.persist(pt, rep);
        backend_interface_retries_on_error.persist(pt, rep);
        backend_interface_retry_interval_secs.persist(pt, rep);
        backend_interface_retry_backoff_multiplier.persist(pt, rep);
        backend_interface_partial_read_nullio.persist(pt, rep);
        backend_interface_switch_connection_pool_policy.persist(pt, rep);
        backend_interface_switch_connection_pool_partial_read_policy.persist(pt, rep);
        backend_interface_switch_connection_pool_on_error_policy.persist(pt, rep);
    }

    DECLARE_PARAMETER(backend_connection_pool_capacity);
    DECLARE_PARAMETER(backend_connection_pool_blacklist_secs);
    DECLARE_PARAMETER(backend_interface_retries_on_error);
    DECLARE_PARAMETER(backend_interface_retry_interval_secs);
    DECLARE_PARAMETER(backend_interface_retry_backoff_multiplier);
    DECLARE_PARAMETER(backend_interface_partial_read_nullio);
    DECLARE_PARAMETER(backend_interface_switch_connection_pool_policy);
    DECLARE_PARAMETER(backend_interface_switch_connection_pool_partial_read_policy);
    DECLARE_PARAMETER(backend_interface_switch_connection_pool_on_error_policy);
};

}

#endif // !BACKEND_CONNECTION_MANAGER_PARAMETERS_H_
