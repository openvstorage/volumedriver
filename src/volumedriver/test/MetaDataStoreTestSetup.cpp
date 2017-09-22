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
                                               const vd::MDSNodeConfigs& mds_node_configs)
    : arakoon_test_setup_(ara_test_setup)
    , mds_node_configs_(mds_node_configs)
{}

std::unique_ptr<vd::MetaDataBackendConfig>
MetaDataStoreTestSetup::make_config()
{
    std::unique_ptr<vd::MetaDataBackendConfig> cfg;

    switch (backend_type_)
    {
    case vd::MetaDataBackendType::Arakoon:
        VERIFY(arakoon_test_setup_ != nullptr);
        cfg =
            std::make_unique<vd::ArakoonMetaDataBackendConfig>(arakoon_test_setup_->clusterID(),
                                                               arakoon_test_setup_->node_configs());
        break;
    case vd::MetaDataBackendType::MDS:
        VERIFY(not mds_node_configs_.empty());
        cfg =
            std::make_unique<vd::MDSMetaDataBackendConfig>(mds_node_configs_,
                                                           vd::ApplyRelocationsToSlaves::T);
        break;
    case vd::MetaDataBackendType::RocksDB:
        cfg = std::make_unique<vd::RocksDBMetaDataBackendConfig>();
        break;
    case vd::MetaDataBackendType::TCBT:
        cfg = std::make_unique<vd::TCBTMetaDataBackendConfig>();
        break;
    }

    return cfg;
}

}
