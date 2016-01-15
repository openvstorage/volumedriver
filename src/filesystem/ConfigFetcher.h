// Copyright 2016 iNuron NV
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

#ifndef VFS_CONFIG_FETCHER_H_
#define VFS_CONFIG_FETCHER_H_

#include <youtils/EtcdUrl.h>
#include <youtils/IOException.h>
#include <youtils/Logging.h>
#include <youtils/VolumeDriverComponent.h>

#include <boost/property_tree/ptree_fwd.hpp>

namespace volumedriverfs
{

class ConfigFetcher
{
public:
    MAKE_EXCEPTION(Exception, fungi::IOException);

    explicit ConfigFetcher(const std::string& config)
        : config_(config)
    {}

    ~ConfigFetcher() = default;

    boost::property_tree::ptree
    operator()(VerifyConfig);

private:
    DECLARE_LOGGER("VFSConfigFetcher");

    std::string config_;
};

}

#endif // !VFS_CONFIG_FETCHER_H_
