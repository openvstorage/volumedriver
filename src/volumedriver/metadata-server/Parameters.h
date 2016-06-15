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

#ifndef META_DATA_SERVER_PARAMETERS_H_
#define META_DATA_SERVER_PARAMETERS_H_

#include "ServerConfig.h"

#include <atomic>
#include <iosfwd>
#include <string>

#include <youtils/InitializedParam.h>
#include <volumedriver/MDSNodeConfig.h>

namespace metadata_server
{

enum class DataBaseType
{
    RocksDb,
};

std::ostream&
operator<<(std::ostream& os,
           DataBaseType t);

std::istream&
operator>>(std::istream& is,
           DataBaseType& t);

}

namespace initialized_params
{

extern const char mds_component_name[];

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(mds_poll_secs,
                                                  std::atomic<uint64_t>);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(mds_db_type,
                                       metadata_server::DataBaseType);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(mds_cached_pages,
                                       uint32_t);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(mds_timeout_secs,
                                       uint32_t);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(mds_threads,
                                       uint32_t);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(mds_bg_threads,
                                       uint32_t);

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(mds_nodes,
                                                  metadata_server::ServerConfigs);

}

#endif // META_DATA_SERVER_PARAMETERS_H_
