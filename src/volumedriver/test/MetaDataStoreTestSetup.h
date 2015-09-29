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

#ifndef VD_TEST_META_DATA_STORE_TEST_SETUP_H_
#define VD_TEST_META_DATA_STORE_TEST_SETUP_H_

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <youtils/AlternativeOptionAgain.h>
#include <youtils/ArakoonTestSetup.h>
#include <youtils/Logging.h>

#include <backend/BackendConnectionManager.h>

#include "../MetaDataBackendConfig.h"

namespace youtils
{
template<> struct
AlternativeOptionTraits<volumedriver::MetaDataBackendType>
{
    static std::map<volumedriver::MetaDataBackendType,
                    const char*> str_;

};
}

namespace volumedrivertest
{

class MDSTestSetup;
class WithMDS;

class MetaDataStoreTestSetup
{
public:
    MetaDataStoreTestSetup(std::shared_ptr<arakoon::ArakoonTestSetup> ara_test_setup,
                           const volumedriver::MDSNodeConfig& mds_node_config);

    ~MetaDataStoreTestSetup() = default;

    MetaDataStoreTestSetup(const MetaDataStoreTestSetup&) = delete;

    MetaDataStoreTestSetup&
    operator=(const MetaDataStoreTestSetup&) = delete;

    std::unique_ptr<volumedriver::MetaDataBackendConfig>
    make_config();

    static volumedriver::MetaDataBackendType backend_type_;

private:
    DECLARE_LOGGER("MetaDataStoreTestSetup");

    const boost::filesystem::path root_;

    std::shared_ptr<arakoon::ArakoonTestSetup> arakoon_test_setup_;
    const volumedriver::MDSNodeConfig mds_node_config_;
};

class ArakoonMDStoreOptions
    : public youtils::AlternativeOptionAgain<volumedriver::MetaDataBackendType,
                                             volumedriver::MetaDataBackendType::Arakoon>
{
public:
    ArakoonMDStoreOptions()
        : port_(0)
    {
        desc_.add_options()
            ("arakoon-binary-path",
             boost::program_options::value<std::string>(&path_)->default_value("/usr/bin/arakoon"),
             "path to arakoon binary")
            ("arakoon-port",
             boost::program_options::value<uint16_t>(&port_)->default_value(12345),
             "start of port range to use for arakoon");
    }

    virtual ~ArakoonMDStoreOptions() = default;

    void
    actions() override
    {
        LOG_INFO("Running the tests with " << id_() <<
                 " metadata stores, using " << path_ << " and base port " << port_);
        arakoon::ArakoonTestSetup::setArakoonBinaryPath(path_);
        arakoon::ArakoonTestSetup::setArakoonBasePort(port_);
        MetaDataStoreTestSetup::backend_type_ = volumedriver::MetaDataBackendType::Arakoon;
    }

    const boost::program_options::options_description&
    options_description() const override
    {
        return desc_;
    }

private:
    DECLARE_LOGGER("ArakoonMDStoreOptions");

    std::string path_;
    uint16_t port_;
    boost::program_options::options_description desc_;
};

class MDSMDStoreOptions
    : public youtils::AlternativeOptionAgain<volumedriver::MetaDataBackendType,
                                             volumedriver::MetaDataBackendType::MDS>
{
public:
    MDSMDStoreOptions()
    {}

    void
    actions() override
    {
        LOG_INFO("Running the tests with " << id_() <<
                 " metadata stores");
        MetaDataStoreTestSetup::backend_type_ = volumedriver::MetaDataBackendType::MDS;
    }

    const boost::program_options::options_description&
    options_description() const override
    {
        return desc_;
    }

private:
    DECLARE_LOGGER("MDSOptions");
    boost::program_options::options_description desc_;
};

class RocksDBMDStoreOptions
    : public youtils::AlternativeOptionAgain<volumedriver::MetaDataBackendType,
                                             volumedriver::MetaDataBackendType::RocksDB>
{
public:
    RocksDBMDStoreOptions()
    {}

    void
    actions() override
    {
        LOG_INFO("Running the tests with " << id_() <<
                 " metadata stores");
        MetaDataStoreTestSetup::backend_type_ = volumedriver::MetaDataBackendType::RocksDB;
    }

    const boost::program_options::options_description&
    options_description() const override
    {
        return desc_;
    }

private:
    DECLARE_LOGGER("RocksDBOptions");
    boost::program_options::options_description desc_;
};

class TCBTMDStoreOptions
    : public youtils::AlternativeOptionAgain<volumedriver::MetaDataBackendType,
                                             volumedriver::MetaDataBackendType::TCBT>

{
public:
    TCBTMDStoreOptions()
    {}

    void
    actions() override
    {
        LOG_INFO("Running the tests with " << id_() <<
                 " metadata stores");
        MetaDataStoreTestSetup::backend_type_ = volumedriver::MetaDataBackendType::TCBT;
    }

    const boost::program_options::options_description&
    options_description() const override
    {
        return desc_;
    }

private:
    DECLARE_LOGGER("TCBTOptions");
    boost::program_options::options_description desc_;
};

typedef LOKI_TYPELIST_4(ArakoonMDStoreOptions,
                        MDSMDStoreOptions,
                        RocksDBMDStoreOptions,
                        TCBTMDStoreOptions)
    metadata_options_t;

}

#endif //!  VD_TEST_META_DATA_STORE_TEST_SETUP_H_
