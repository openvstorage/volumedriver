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

#ifndef BACKENDPARAMETERS_H_
#define BACKENDPARAMETERS_H_

#include <alba/proxy_client.h>

#include <youtils/InitializedParam.h>

namespace backend
{

enum class BackendType
{
    LOCAL,
    S3,
    MULTI,
    ALBA
};

std::ostream&
operator<<(std::ostream& os,
           BackendType t);

std::istream&
operator>>(std::istream& is,
           BackendType& t);

enum class S3Flavour
{
    S3, // Amazon / generic S3 backend
    GCS, // Google Cloud Storage
    WALRUS, // Eucalyptus Walrus
    SWIFT // OpenStack Swift
};

std::ostream&
operator<<(std::ostream& os,
           S3Flavour t);

std::istream&
operator>>(std::istream& is,
           S3Flavour& t);

}

namespace initialized_params
{

const char backend_connection_manager_name[] = "backend_connection_manager";

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(backend_connection_pool_capacity,
                                                  uint32_t);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(backend_interface_retries_on_error,
                                                  std::atomic<uint32_t>);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(backend_interface_retry_interval_secs,
                                                  std::atomic<uint32_t>);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(backend_interface_retry_backoff_multiplier,
                                                  std::atomic<double>);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(backend_interface_partial_read_nullio,
                                       bool);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(backend_type, backend::BackendType);
DECLARE_INITIALIZED_PARAM(local_connection_path, std::string);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(local_connection_tv_sec, int);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(local_connection_tv_nsec, int);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(local_connection_enable_partial_read, bool);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(local_connection_sync_object_after_write, bool);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_host, std::string);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_username, std::string);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_password, std::string);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_verbose_logging, bool);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_port, uint16_t);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_use_ssl, bool);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_ssl_verify_host, bool);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_ssl_cert_file, std::string);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_flavour, backend::S3Flavour);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_strict_consistency, bool);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(alba_connection_host, std::string);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(alba_connection_port, uint16_t);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(alba_connection_timeout, uint16_t);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(alba_connection_preset, std::string);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(alba_connection_transport, alba::proxy_client::Transport);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(alba_connection_use_rora, bool);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(alba_connection_rora_manifest_cache_capacity, size_t);

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(bgc_threads, uint32_t);

}

#endif /* BACKENDPARAMETERS_H_ */

// Local Variables: **
// mode: c++ **
// End: **
