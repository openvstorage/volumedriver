// Copyright 2015 Open vStorage NV
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

}
