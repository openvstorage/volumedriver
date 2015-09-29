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

#include "BackendNamesFilter.h"
#include "FailOverCacheConfigWrapper.h"
#include "SCOAccessData.h"
#include "SnapshotManagement.h"
#include "VolumeConfig.h"
#include "Types.h"

namespace volumedriver
{

bool
BackendNamesFilter::operator()(const std::string& name) const
{
    return boost::regex_match(name, regex_());
}

const boost::regex&
BackendNamesFilter::regex_()
{
    // we don't use uppercase hex digits in sco / tlog names, so no [[:xdigit:]] but rather
    // [0-9a-f] below.
    static const std::string rexstr(std::string(SCOAccessDataPersistor::backend_name)
                                   + std::string("|")
                                   + FailOverCacheConfigWrapper::config_backend_name
                                   + std::string("|")
                                   + VolumeConfig::config_backend_name
                                   + std::string("|")
                                   + snapshotFilename()
                                   + std::string("|")
                                   + std::string("tlog_[0-9a-f]{8}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{12}")
                                   + std::string("|")
                                   + std::string("[0-9a-f]{2}_[0-9a-f]{8}_[0-9a-f]{2}")
                                   );

    static const boost::regex rex(rexstr, boost::regex::extended);
    return rex;
}

}

// Local Variables: **
// mode: c++ **
// End: **
