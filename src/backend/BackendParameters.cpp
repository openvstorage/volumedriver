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

#include "BackendConfig.h"
#include "BackendParameters.h"
#include "GarbageCollector.h"

#include <boost/bimap.hpp>

#include <youtils/InitializedParam.h>
#include <youtils/StreamUtils.h>

using namespace std::literals::string_literals;

namespace backend
{

namespace yt = youtils;

namespace
{
// Hack around boost::bimap not supporting initializer lists by building a vector first
// and then filling the bimap from it. And since we don't want the vector to stick around
// forever we use a function.

void
backend_reminder(BackendType t) __attribute__((unused));

void
backend_reminder(BackendType t)
{
    switch (t)
    {
    case BackendType::LOCAL:
    case BackendType::S3:
    case BackendType::MULTI:
    case BackendType::ALBA:
        // If the compiler yells at you that you've forgotten dealing with an enum
        // value here chances are that it's also missing from the translations map
        // below. If so add it NOW.
        break;
    }
}

typedef boost::bimap<BackendType, std::string> BackendTranslationsMap;

BackendTranslationsMap
init_backend_translations()
{
    const std::vector<BackendTranslationsMap::value_type> initv{
        { BackendType::LOCAL, "LOCAL" },
        { BackendType::S3, "S3" },
        { BackendType::MULTI, "MULTI" },
        { BackendType::ALBA, "ALBA" }
    };

    return BackendTranslationsMap(initv.begin(), initv.end());
};

void
s3_reminder(S3Flavour t) __attribute__((unused));

void
s3_reminder(S3Flavour t)
{
    switch (t)
    {
    case S3Flavour::S3:
    case S3Flavour::GCS:
    case S3Flavour::WALRUS:
    case S3Flavour::SWIFT:
        break;
        // If the compiler yells at you that you've forgotten dealing with an enum
        // value here chances are that it's also missing from the translations map
        // below. If so add it NOW.
    }
}

typedef boost::bimap<S3Flavour, std::string> S3TranslationsMap;

S3TranslationsMap
init_s3_translations()
{
    const std::vector<S3TranslationsMap::value_type> initv{
        { S3Flavour::S3, "S3" },
        { S3Flavour::GCS, "GCS" },
        { S3Flavour::WALRUS, "WALRUS" },
        { S3Flavour::SWIFT, "SWIFT" }
    };

    return S3TranslationsMap(initv.begin(), initv.end());
};

}

std::ostream&
operator<<(std::ostream& os,
           BackendType t)
{
    static const BackendTranslationsMap
        backend_translations(init_backend_translations());
    return yt::StreamUtils::stream_out(backend_translations.left, os, t);
}

std::istream&
operator>>(std::istream& is,
           BackendType& t)
{
    static const BackendTranslationsMap
        backend_translations(init_backend_translations());
    return yt::StreamUtils::stream_in(backend_translations.right, is, t);
}

std::ostream&
operator<<(std::ostream& os,
           S3Flavour t)
{
    static const S3TranslationsMap s3_translations(init_s3_translations());
    return yt::StreamUtils::stream_out(s3_translations.left, os, t);
}

std::istream&
operator>>(std::istream& is,
           S3Flavour& t)
{
    static const S3TranslationsMap s3_translations(init_s3_translations());
    return yt::StreamUtils::stream_in(s3_translations.right, is, t);
}

}

//TODO [BDV] cleanup
//to avoid "specialization of InitializedParameterTraits ... in different namespaces"
namespace initialized_params
{

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(backend_connection_pool_capacity,
                                      backend_connection_manager_name,
                                      "backend_connection_pool_capacity",
                                      "Capacity of the connection pool maintained by the BackendConnectionManager",
                                      ShowDocumentation::T,
                                      64);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(backend_interface_retries_on_error,
                                      backend_connection_manager_name,
                                      "backend_interface_retries_on_error",
                                      "How many times to retry a failed backend operation",
                                      ShowDocumentation::T,
                                      1U);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(backend_interface_retry_interval_secs,
                                      backend_connection_manager_name,
                                      "backend_interface_retry_interval_secs",
                                      "delay before retrying a failed backend operation in seconds",
                                      ShowDocumentation::T,
                                      0U);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(backend_interface_retry_backoff_multiplier,
                                      backend_connection_manager_name,
                                      "backend_interface_retry_backoff_multiplier",
                                      "multiplier for the retry interval on each subsequent retry",
                                      ShowDocumentation::T,
                                      1.0);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(backend_type,
                                      backend_connection_manager_name,
                                      "backend_type",
                                      "Type of backend connection one of ALBA, LOCAL, or S3, the other parameters in this section are only used when their correct backendtype is set",
                                      ShowDocumentation::T,
                                      backend::BackendType::LOCAL);

// Local Backend Parameters
DEFINE_INITIALIZED_PARAM(local_connection_path,
                         backend_connection_manager_name,
                         "local_connection_path",
                         "When backend_type is LOCAL: path to use as LOCAL backend, otherwise ignored",
                         ShowDocumentation::T);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(local_connection_tv_sec,
                                      backend_connection_manager_name,
                                      "local_connection_tv_sec",
                                      "When backend_type is LOCAL, seconds of delay to use when writing to the backend, otherwise ignored",
                                      ShowDocumentation::F,
                                      0);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(local_connection_tv_nsec,
                                      backend_connection_manager_name,
                                      "local_connection_tv_nsec",
                                      "When backend_type is LOCAL, nano_seconds delay to use when writing to the backend, otherwise ignored",
                                      ShowDocumentation::F,
                                      0);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(local_connection_enable_partial_read,
                                      backend_connection_manager_name,
                                      "local_connection_enable_partial_read",
                                      "Enable/Disable partial read support",
                                      ShowDocumentation::F,
                                      true);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(local_connection_sync_object_after_write,
                                      backend_connection_manager_name,
                                      "local_connection_sync_object_after_write",
                                      "Enable/Disable sync'ing after each object write",
                                      ShowDocumentation::F,
                                      true);

#define S3_DEFAULT_HOSTNAME                "s3.amazonaws.com"s

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_host,
                                      backend_connection_manager_name,
                                      "s3_connection_host",
                                      "When backend_type is S3: the S3 host to connect to, otherwise ignored",
                                      ShowDocumentation::T,
                                      S3_DEFAULT_HOSTNAME);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_port,
                                      backend_connection_manager_name,
                                      "s3_connection_port",
                                      "When backend_type is S3: the S3 port to connect to, otherwise ignored",
                                      ShowDocumentation::T,
                                      80);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_username,
                                      backend_connection_manager_name,
                                      "s3_connection_username",
                                      "When backend_type is S3: the S3 username, otherwise ignored",
                                      ShowDocumentation::T,
                                      ""s);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_password,
                                      backend_connection_manager_name,
                                      "s3_connection_password",
                                      "When backend_type is S3: the S3 password",
                                      ShowDocumentation::T,
                                      ""s);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_verbose_logging,
                                      backend_connection_manager_name,
                                      "s3_connection_verbose_logging",
                                      "When backend_type is S3: whether to do verbose logging",
                                      ShowDocumentation::T,
                                      true);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_use_ssl,
                                      backend_connection_manager_name,
                                      "s3_connection_use_ssl",
                                      "When backend_type is S3: whether to use SSL to encrypt the connection",
                                      ShowDocumentation::T,
                                      false);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_ssl_verify_host,
                                      backend_connection_manager_name,
                                      "s3_connection_ssl_verify_host",
                                      "When backend_type is S3: whether to verify the SSL certificate's subject against the host",
                                      ShowDocumentation::T,
                                      true);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_ssl_cert_file,
                                      backend_connection_manager_name,
                                      "s3_connection_ssl_cert_file",
                                      "When backend_type is S3: path to a file holding the SSL certificate",
                                      ShowDocumentation::T,
                                      ""s);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_flavour,
                                      backend_connection_manager_name,
                                      "s3_connection_flavour",
                                      "S3 backend flavour: S3 (default), GCS, WALRUS or SWIFT",
                                      ShowDocumentation::T,
                                      backend::S3Flavour::S3);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(s3_connection_strict_consistency,
                                      backend_connection_manager_name,
                                      "s3_connection_consistency",
                                      "Override the default assumption of eventual consistency for S3 backends",
                                      ShowDocumentation::F,
                                      false);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(alba_connection_host,
                                      backend_connection_manager_name,
                                      "alba_connection_host",
                                      "When backend_type is ALBA: the ALBA host to connect to, otherwise ignored",
                                      ShowDocumentation::T,
                                      "127.0.0.1"s);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(alba_connection_port,
                                      backend_connection_manager_name,
                                      "alba_connection_port",
                                      "When the backend_type is ALBA: The ALBA port to connect to, otherwise ignored",
                                      ShowDocumentation::T,
                                      56789);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(alba_connection_timeout,
                                      backend_connection_manager_name,
                                      "alba_connection_timeout",
                                      "The timeout for the ALBA proxy, in seconds",
                                      ShowDocumentation::T,
                                      5);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(alba_connection_preset,
                                      backend_connection_manager_name,
                                      "alba_connection_preset",
                                      "When backend_type is ALBA: the ALBA preset to use for new namespaces",
                                      ShowDocumentation::T,
                                      ""s);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(alba_connection_transport,
                                      backend_connection_manager_name,
                                      "alba_connection_transport",
                                      "When backend_type is ALBA: the ALBA connection to use: TCP (default) or RDMA",
                                      ShowDocumentation::T,
                                      alba::proxy_client::Transport::tcp);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(bgc_threads,
                                      backend::GarbageCollector::name(),
                                      "bgc_threads",
                                      "Number of threads employed by the BackendGarbageCollector",
                                      ShowDocumentation::T,
                                      4);

}

// Local Variables: **
// mode: c++ **
// End: **
