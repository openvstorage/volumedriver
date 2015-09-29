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

#include "../MetaDataBackendConfig.h"

#include <sstream>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/unique_ptr.hpp>

#include <youtils/Serialization.h>
#include <youtils/TestBase.h>

namespace volumedrivertest
{

namespace ba = boost::archive;
namespace vd = volumedriver;
namespace ytt = youtilstest;

class MetaDataBackendConfigTest
    : public ytt::TestBase
{
protected:
    template<typename IArchive,
             typename OArchive>
    void
    test_serialization()
    {
        std::stringstream ss;

        const std::string addr("127.0.0.1");
        const uint16_t port = 12345;

        {
            OArchive oa(ss);

            boost::shared_ptr<vd::MetaDataBackendConfig> mdb;

            mdb.reset(new vd::TCBTMetaDataBackendConfig());
            oa << mdb;

            mdb.reset(new vd::RocksDBMetaDataBackendConfig());
            oa << mdb;

            std::vector<vd::MDSNodeConfig> nodes{ vd::MDSNodeConfig(addr,
                                                                    port) };
            mdb.reset(new vd::MDSMetaDataBackendConfig(nodes,
                                                       vd::ApplyRelocationsToSlaves::T));
            oa << mdb;

            mdb.reset(new vd::MDSMetaDataBackendConfig(nodes,
                                                       vd::ApplyRelocationsToSlaves::F));
            oa << mdb;
        }

        IArchive ia(ss);

        boost::shared_ptr<vd::MetaDataBackendConfig> mdb;
        ia >> mdb;

        {
            ASSERT_EQ(vd::MetaDataBackendType::TCBT,
                      mdb->backend_type());
            ASSERT_TRUE(boost::dynamic_pointer_cast<vd::TCBTMetaDataBackendConfig>(mdb) != nullptr);
            mdb.reset();
        }

        ia >> mdb;

        {
            ASSERT_EQ(vd::MetaDataBackendType::RocksDB,
                      mdb->backend_type());
            ASSERT_TRUE(boost::dynamic_pointer_cast<vd::RocksDBMetaDataBackendConfig>(mdb) != nullptr);
            mdb.reset();
        }

        ia >> mdb;

        {
            ASSERT_EQ(vd::MetaDataBackendType::MDS,
                      mdb->backend_type());
            auto mdscfg(boost::dynamic_pointer_cast<vd::MDSMetaDataBackendConfig>(mdb));
            ASSERT_TRUE(mdscfg != nullptr);
            ASSERT_EQ(1,
                      mdscfg->node_configs().size());
            ASSERT_EQ(addr,
                      mdscfg->node_configs()[0].address());
            ASSERT_EQ(port,
                      mdscfg->node_configs()[0].port());
            ASSERT_EQ(vd::ApplyRelocationsToSlaves::T,
                      mdscfg->apply_relocations_to_slaves());
            mdb.reset();
        }

        ia >> mdb;

        {
            ASSERT_EQ(vd::MetaDataBackendType::MDS,
                      mdb->backend_type());
            auto mdscfg(boost::dynamic_pointer_cast<vd::MDSMetaDataBackendConfig>(mdb));
            ASSERT_TRUE(mdscfg != nullptr);
            ASSERT_EQ(1,
                      mdscfg->node_configs().size());
            ASSERT_EQ(addr,
                      mdscfg->node_configs()[0].address());
            ASSERT_EQ(port,
                      mdscfg->node_configs()[0].port());
            ASSERT_EQ(vd::ApplyRelocationsToSlaves::F,
                      mdscfg->apply_relocations_to_slaves());
            mdb.reset();
        }
    }
};

TEST_F(MetaDataBackendConfigTest, text_serialization)
{
    test_serialization<ba::text_iarchive,
                       ba::text_oarchive>();
}

TEST_F(MetaDataBackendConfigTest, binary_serialization)
{
    test_serialization<ba::binary_iarchive,
                       ba::binary_oarchive>();
}

TEST_F(MetaDataBackendConfigTest, unique_ptr_serialization)
{
    const vd::MDSMetaDataBackendConfig::NodeConfigs ncfgs{ vd::MDSNodeConfig("localhost",
                                                                             12345) };

    std::stringstream ss;

    {
        std::unique_ptr<vd::MetaDataBackendConfig>
            cfg(new vd::MDSMetaDataBackendConfig(ncfgs,
                                                 vd::ApplyRelocationsToSlaves::T));
        ba::binary_oarchive oa(ss);
        oa << cfg;
    }

    ba::binary_iarchive ia(ss);
    std::unique_ptr<vd::MetaDataBackendConfig> cfg;
    ia >> cfg;

    EXPECT_TRUE(ncfgs == dynamic_cast<const vd::MDSMetaDataBackendConfig*>(cfg.get())->node_configs());
}

TEST_F(MetaDataBackendConfigTest, enum_streaming)
{
    auto test([&](vd::MetaDataBackendType t)
              {
                  std::stringstream ss;
                  ss << t;

                  vd::MetaDataBackendType u;
                  ss >> u;

                  ASSERT_EQ(t, u);
              });

    test(vd::MetaDataBackendType::Arakoon);
    test(vd::MetaDataBackendType::MDS);
    test(vd::MetaDataBackendType::RocksDB);
    test(vd::MetaDataBackendType::TCBT);
}

}
