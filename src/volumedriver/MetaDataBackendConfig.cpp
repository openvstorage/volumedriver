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

#include "MetaDataBackendConfig.h"
#include "SerializationExports.h"

#include <iostream>
#include <vector>

#include <boost/bimap.hpp>

#include <youtils/StreamUtils.h>

namespace volumedriver
{

namespace yt = youtils;

namespace
{

void
reminder(MetaDataBackendType t) __attribute__((unused));

void
reminder(MetaDataBackendType t)
{
    switch (t)
    {
    case MetaDataBackendType::Arakoon:
    case MetaDataBackendType::MDS:
    case MetaDataBackendType::RocksDB:
    case MetaDataBackendType::TCBT:
        // If the compiler yells at you that you've forgotten dealing with an enum
        // value here chances are that it's also missing from the translations map
        // below. If so add it NOW.
        break;
    }
}

typedef boost::bimap<MetaDataBackendType, std::string> TranslationsMap;

TranslationsMap
init_translations()
{
    const std::vector<TranslationsMap::value_type> initv{
        { MetaDataBackendType::Arakoon, "ARAKOON" },
        { MetaDataBackendType::MDS, "MDS" },
        { MetaDataBackendType::RocksDB, "ROCKSDB" },
        { MetaDataBackendType::TCBT, "TCBT" },
    };

    return TranslationsMap(initv.begin(),
                           initv.end());
}

}

std::ostream&
operator<<(std::ostream& os,
           MetaDataBackendType t)
{
    static const TranslationsMap translations(init_translations());
    return yt::StreamUtils::stream_out(translations.left,
                                       os,
                                       t);
}

std::istream&
operator>>(std::istream& is,
           MetaDataBackendType& t)
{
    static const TranslationsMap translations(init_translations());
    return yt::StreamUtils::stream_in(translations.right,
                                      is,
                                      t);
}

unsigned
MDSMetaDataBackendConfig::default_timeout_secs_ = 20;

}
