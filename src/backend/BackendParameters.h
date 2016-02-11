// Copyright 2015 iNuron NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef BACKENDPARAMETERS_H_
#define BACKENDPARAMETERS_H_

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

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(bgc_threads, uint32_t);

}

#endif /* BACKENDPARAMETERS_H_ */

// Local Variables: **
// mode: c++ **
// End: **
