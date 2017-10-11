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

#include "../MetaDataBackendConfig.h"

#include <sstream>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/unique_ptr.hpp>

#include <youtils/ArchiveTraits.h>
#include <youtils/Serialization.h>
#include <gtest/gtest.h>

namespace volumedrivertest
{

namespace ba = boost::archive;
namespace vd = volumedriver;
namespace yt = youtils;

class MetaDataBackendConfigTest
    : public testing::Test
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
        const uint32_t max_tlogs_behind = 123;
        const unsigned mds_timeout_secs = 10;

        {
            OArchive oa(ss);

            boost::shared_ptr<vd::MetaDataBackendConfig> mdb;

            mdb.reset(new vd::TCBTMetaDataBackendConfig());
            oa << BOOST_SERIALIZATION_NVP(mdb);

            mdb.reset(new vd::RocksDBMetaDataBackendConfig());
            oa << BOOST_SERIALIZATION_NVP(mdb);

            std::vector<vd::MDSNodeConfig> nodes{ vd::MDSNodeConfig(addr,
                                                                    port) };
            mdb.reset(new vd::MDSMetaDataBackendConfig(nodes,
                                                       vd::ApplyRelocationsToSlaves::T));
            oa << BOOST_SERIALIZATION_NVP(mdb);

            mdb.reset(new vd::MDSMetaDataBackendConfig(nodes,
                                                       vd::ApplyRelocationsToSlaves::F,
                                                       mds_timeout_secs,
                                                       max_tlogs_behind));
            oa << BOOST_SERIALIZATION_NVP(mdb);
        }

        IArchive ia(ss);

        boost::shared_ptr<vd::MetaDataBackendConfig> mdb;
        ia >> BOOST_SERIALIZATION_NVP(mdb);

        {
            ASSERT_EQ(vd::MetaDataBackendType::TCBT,
                      mdb->backend_type());
            ASSERT_TRUE(boost::dynamic_pointer_cast<vd::TCBTMetaDataBackendConfig>(mdb) != nullptr);
            mdb.reset();
        }

        ia >> BOOST_SERIALIZATION_NVP(mdb);

        {
            ASSERT_EQ(vd::MetaDataBackendType::RocksDB,
                      mdb->backend_type());
            ASSERT_TRUE(boost::dynamic_pointer_cast<vd::RocksDBMetaDataBackendConfig>(mdb) != nullptr);
            mdb.reset();
        }

        ia >> BOOST_SERIALIZATION_NVP(mdb);

        {
            ASSERT_EQ(vd::MetaDataBackendType::MDS,
                      mdb->backend_type());
            auto mdscfg(boost::dynamic_pointer_cast<vd::MDSMetaDataBackendConfig>(mdb));
            ASSERT_TRUE(mdscfg != nullptr);
            ASSERT_EQ(1U,
                      mdscfg->node_configs().size());
            ASSERT_EQ(addr,
                      mdscfg->node_configs()[0].address());
            ASSERT_EQ(port,
                      mdscfg->node_configs()[0].port());
            ASSERT_EQ(vd::ApplyRelocationsToSlaves::T,
                      mdscfg->apply_relocations_to_slaves());
            ASSERT_EQ(boost::none,
                      mdscfg->max_tlogs_behind());
            mdb.reset();
        }

        ia >> BOOST_SERIALIZATION_NVP(mdb);

        {
            ASSERT_EQ(vd::MetaDataBackendType::MDS,
                      mdb->backend_type());
            auto mdscfg(boost::dynamic_pointer_cast<vd::MDSMetaDataBackendConfig>(mdb));
            ASSERT_TRUE(mdscfg != nullptr);
            ASSERT_EQ(1U,
                      mdscfg->node_configs().size());
            ASSERT_EQ(addr,
                      mdscfg->node_configs()[0].address());
            ASSERT_EQ(port,
                      mdscfg->node_configs()[0].port());
            ASSERT_EQ(vd::ApplyRelocationsToSlaves::F,
                      mdscfg->apply_relocations_to_slaves());
            ASSERT_EQ(std::chrono::seconds(mds_timeout_secs),
                      mdscfg->timeout());
            if (yt::IsForwardCompatibleArchive<OArchive>::value)
            {
                ASSERT_TRUE(yt::IsForwardCompatibleArchive<IArchive>::value);
                ASSERT_NE(boost::none,
                          mdscfg->max_tlogs_behind());
                ASSERT_EQ(max_tlogs_behind,
                          *mdscfg->max_tlogs_behind());
            }
            else
            {
                ASSERT_FALSE(yt::IsForwardCompatibleArchive<IArchive>::value);
                ASSERT_EQ(boost::none,
                          mdscfg->max_tlogs_behind());
            }

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

TEST_F(MetaDataBackendConfigTest, xml_serialization)
{
    test_serialization<ba::xml_iarchive,
                       ba::xml_oarchive>();
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
