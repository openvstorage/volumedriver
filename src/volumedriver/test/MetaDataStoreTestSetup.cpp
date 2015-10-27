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

#include "MDSTestSetup.h"
#include "MetaDataStoreTestSetup.h"

#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include <youtils/ArakoonTestSetup.h>
#include <youtils/System.h>

namespace youtils
{

using opt_traits_t = struct
    AlternativeOptionTraits<volumedriver::MetaDataBackendType> ;


std::map<volumedriver::MetaDataBackendType, const char*>
opt_traits_t::str_ = {
    {
        volumedriver::MetaDataBackendType::Arakoon, "ARAKOON"
    },
    {
        volumedriver::MetaDataBackendType::MDS, "MDS"
    },
    {
        volumedriver::MetaDataBackendType::RocksDB, "ROCKSDB"
    },
    {
        volumedriver::MetaDataBackendType::TCBT, "TCBT"
    }
};
}

namespace volumedrivertest
{

namespace ara = arakoon;
namespace be = backend;
namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace vd = volumedriver;
namespace yt = youtils;

vd::MetaDataBackendType
MetaDataStoreTestSetup::backend_type_ =
    yt::System::get_env_with_default<vd::MetaDataBackendType>("METADATA_BACKEND_TYPE",
                                                              vd::MetaDataBackendType::MDS);

MetaDataStoreTestSetup::MetaDataStoreTestSetup(std::shared_ptr<ara::ArakoonTestSetup> ara_test_setup,
                                               const vd::MDSNodeConfig& mds_node_config)
    : arakoon_test_setup_(ara_test_setup)
    , mds_node_config_(mds_node_config)
{}

std::unique_ptr<vd::MetaDataBackendConfig>
MetaDataStoreTestSetup::make_config()
{
    std::unique_ptr<vd::MetaDataBackendConfig> cfg;

    switch (backend_type_)
    {
    case vd::MetaDataBackendType::Arakoon:
        VERIFY(arakoon_test_setup_ != nullptr);
        cfg.reset(new vd::ArakoonMetaDataBackendConfig(arakoon_test_setup_->clusterID(),
                                                       arakoon_test_setup_->node_configs()));
        break;
    case vd::MetaDataBackendType::MDS:
        {
            const vd::MDSNodeConfigs ncfgs{ mds_node_config_ };
            cfg.reset(new vd::MDSMetaDataBackendConfig(ncfgs,
                                                       vd::ApplyRelocationsToSlaves::T));
            break;
        }
    case vd::MetaDataBackendType::RocksDB:
        cfg.reset(new vd::RocksDBMetaDataBackendConfig());
        break;
    case vd::MetaDataBackendType::TCBT:
        cfg.reset(new vd::TCBTMetaDataBackendConfig());
        break;
    }

    return cfg;
}

}
