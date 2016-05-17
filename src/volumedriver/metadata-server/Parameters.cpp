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

#include "Parameters.h"
#include "ServerConfig.h"

#include <iostream>

#include <boost/bimap.hpp>

#include <youtils/Assert.h>
#include <youtils/StreamUtils.h>

namespace metadata_server
{

namespace yt = youtils;

namespace
{

void
database_type_reminder(DataBaseType t) __attribute__((unused));

void
database_type_reminder(DataBaseType t)
{
    switch (t)
    {
    case DataBaseType::RocksDb:
        // If the compiler yells at you that you've forgotten dealing with an enum
        // value chances are that it's also missing from the translations map below.
        // If so add it RIGHT NOW.
        break;
    }
}

typedef boost::bimap<DataBaseType, std::string> TranslationsMap;

// Hack around boost::bimap not supporting initializer lists by building a vector first
// and then filling the bimap from it. And since we don't want the vector to stick around
// forever we use a function.
TranslationsMap
init_translations()
{
    const std::vector<TranslationsMap::value_type> initv{
        { DataBaseType::RocksDb, "ROCKSDB" }
    };

    return TranslationsMap(initv.begin(),
                           initv.end());
}

const TranslationsMap translations(init_translations());

}

std::ostream&
operator<<(std::ostream& os,
           DataBaseType t)
{
    return yt::StreamUtils::stream_out(translations.left,
                                       os,
                                       t);
}

std::istream&
operator>>(std::istream& is,
           DataBaseType& t)
{
    return yt::StreamUtils::stream_in(translations.right,
                                      is,
                                      t);
}

}

namespace initialized_params
{

namespace mds = metadata_server;
namespace vd = volumedriver;

using namespace std::literals::string_literals;

const char mds_component_name[] = "metadata_server";

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(mds_db_type,
                                      mds_component_name,
                                      "mds_db_type",
                                      "Type of database to use for metadata. Supported values: ROCKSDB",
                                      ShowDocumentation::T,
                                      mds::DataBaseType::RocksDb);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(mds_cached_pages,
                                      mds_component_name,
                                      "mds_cached_pages",
                                      "Capacity of the metadata page cache per volume",
                                      ShowDocumentation::T,
                                      256);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(mds_poll_secs,
                                      mds_component_name,
                                      "mds_poll_secs",
                                      "Poll interval for the backend check in seconds",
                                      ShowDocumentation::T,
                                      300);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(mds_timeout_secs,
                                      mds_component_name,
                                      "mds_timeout_secs",
                                      "Timeout for network transfers - (0 -> no timeout!)",
                                      ShowDocumentation::T,
                                      30);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(mds_threads,
                                      mds_component_name,
                                      "mds_threads",
                                      "Number of threads per node (0 -> autoconfiguration based on the number of available CPUs)",
                                      ShowDocumentation::T,
                                      1);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(mds_bg_threads,
                                      mds_component_name,
                                      "mds_bg_threads",
                                      "Number of MDS background threads per node (0 -> autoconfiguration based on the number of available CPUs)",
                                      ShowDocumentation::F,
                                      4);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(mds_nodes,
                                      mds_component_name,
                                      "mds_nodes",
                                      "an array of MDS node configurations each containing address, port, db_directory and scratch_directory",
                                      ShowDocumentation::T,
                                      mds::ServerConfigs());
}
