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

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(mds_nodes,
                                                  metadata_server::ServerConfigs);

}

#endif // META_DATA_SERVER_PARAMETERS_H_
