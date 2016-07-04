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

#include "TestWithBackendMainHelper.h"
#include <youtils/ScriptFile.h>
#include <youtils/System.h>
#include <youtils/FileUtils.h>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/ptr_container/ptr_map.hpp>

#include <youtils/BuildInfo.h>
#include "gtest/gtest.h"
#include <youtils/Logger.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include "BackendConfig.h"
#include "BackendParameters.h"
#include <pstreams/pstream.h>
#include <youtils/prudence/prudence.h>
namespace backend
{


struct AlbaConfigurationData : public BackendConfigurationData
{
    AlbaConfigurationData(const std::vector<std::string>& prudence_args)
    {
        prudence_pstream = std::make_unique<redi::ipstream>(prudence_args);
        while(not prudence_pstream->is_open())
        {
            sleep(1);
        }
        // ME NO SLO.. ME JUST OCMEL PROGRAM
        sleep(10);

        ASSERT(not prudence_pstream->rdbuf()->exited());
    }
    virtual ~AlbaConfigurationData()
    {}

    std::unique_ptr<redi::ipstream> prudence_pstream;

};


namespace fs = boost::filesystem;
TestWithBackendMainHelper::TestWithBackendMainHelper(int argc,
                                                     char** argv)
    : TestMainHelper(argc,
                     argv)
    , backend_setup_options_("Backend Setup Options")
{
    backend_setup_options_.add_options()
        ("skip-backend-setup",
         po::value<bool>(&skip_backend_setup_)->default_value(false),
         "whether to skip the backend setup");


}

std::unique_ptr<BackendConfig>
TestWithBackendMainHelper::backend_config_;

void
TestWithBackendMainHelper::setupTestBackend()
{
    boost::property_tree::ptree pt;

    if(backend_config_file_)
    {
        boost::property_tree::json_parser::read_json(youtils::MainHelper::backend_config_file_->string(),
                                                     pt);
        auto backend_config_ = BackendConfig::makeBackendConfig(pt);
        using namespace std::string_literals;

        if(skip_backend_setup_)
        {
            LOG_INFO("Skipping backend setup phase");
        }
        else
        {
            LOG_INFO("doing backend setup phase");

            // Here we run setup code related to each backend
            switch (backend_config_->backend_type.value())
            {
            case BackendType::LOCAL:
                check_and_setup_local();
                break;
            case BackendType::S3:
                check_and_setup_S3();
                break;
            case BackendType::MULTI:
                check_and_setup_MULTI();
                break;
            case BackendType::ALBA:
                check_and_setup_ALBA();
                break;
            }
        }
    }
    else
    {
        initialized_params::PARAMETER_TYPE(backend_type)(BackendType::LOCAL).persist(pt);
        fs::path local_backend_path = youtils::FileUtils::temp_path() / "localbackend";
        initialized_params::PARAMETER_TYPE(local_connection_path)(local_backend_path.string()).persist(pt);
        const fs::path p(local_backend_path);
        LOG_INFO("Removing and recreating directory for localbackend " << p);
        fs::remove_all(p);
        fs::create_directories(p);
    }
    backend_config_ = BackendConfig::makeBackendConfig(pt);
}

void
TestWithBackendMainHelper::tearDownTestBackend()
{

    if(backend_config_)
    {
        if(skip_backend_setup_)
        {
            LOG_INFO("Skipping backend setup phase");

        }
        else
        {
            LOG_INFO("Doing the backend setup phase");
            switch (backend_config_->backend_type.value())
            {
            case BackendType::LOCAL:
                teardown_local();
                break;
            case BackendType::S3:
                teardown_S3();
                break;
            case BackendType::MULTI:
                teardown_MULTI();
                break;
            case BackendType::ALBA:
                teardown_ALBA();
                break;
            }
        }
    }
    else
    {
        LOG_ERROR("No backend config, no teardown baby.");
    }
}

void
TestWithBackendMainHelper::check_and_setup_local()
{}

void
TestWithBackendMainHelper::check_and_setup_S3()
{}

void
TestWithBackendMainHelper::check_and_setup_MULTI()
{}

void
TestWithBackendMainHelper::check_and_setup_ALBA()
{
    using namespace std::string_literals;
    using namespace youtils;

    LOG_INFO("Setting up ALBA backend");

    fs::path prudence_alba_log =
        FileUtils::create_temp_file_in_temp_dir("prudence-alba.log");

    fs::path alba_config_dir =
        FileUtils::create_temp_dir_in_temp_dir("alba_configuration");
    System::set_env("ALBA_CONFIG_DIR",
                    alba_config_dir.string(),
                    true);
    fs::path temp_path(FileUtils::temp_path());
    fs::path cleanup_script = FileUtils::create_temp_file_in_temp_dir("alba_cleanup_script");
    std::vector<std::string> cleanup_script_lines;
    cleanup_script_lines.push_back("rm -rf "s + alba_config_dir.string());
    cleanup_script_lines.push_back("rm -rf "s + cleanup_script.string());
    cleanup_script_lines.push_back("rm -rf "s + prudence_alba_log.string());
    cleanup_script_lines.push_back("( exec alba-stop-script.sh )");
    ScriptFile p(cleanup_script,
                 cleanup_script_lines);
    std::vector<std::string> argv;
    argv.push_back("prudence");
    argv.push_back("--start-script=alba-start-script.sh");
    argv.push_back("--cleanup-script="s + cleanup_script.string());
    argv.push_back("--logsink="s + prudence_alba_log.string());
    argv.push_back("--loglevel=info");

    backend_config_data_ = std::make_unique<AlbaConfigurationData>(argv);
}

void
TestWithBackendMainHelper::teardown_local()
{}

void
TestWithBackendMainHelper::teardown_S3()
{}

void
TestWithBackendMainHelper::teardown_MULTI()
{}

void
TestWithBackendMainHelper::teardown_ALBA()
{
    if(backend_config_data_)
    {
        LOG_INFO("Cleaning up ALBA backend");
        AlbaConfigurationData* data = dynamic_cast<AlbaConfigurationData*>(backend_config_data_.get());
        ASSERT(data);
        data->prudence_pstream->rdbuf()->kill(parent_dies_signal);
        std::string line;
        while(std::getline(*(data->prudence_pstream),
                           line))
        {
            LOG_INFO(line);
        }
        sleep(1);

        EXPECT_TRUE(data->prudence_pstream->rdbuf()->exited());
        // REMOVE ALBA_PRUDENCE LOG HERE EVENTUALLY
    }
}

}
