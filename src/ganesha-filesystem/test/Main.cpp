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

#include "GaneshaTestSetup.h"

#include <string>
#include <iostream>
#include <sstream>
#include <cassert>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/ptr_container/ptr_map.hpp>

#include <youtils/BuildInfoString.h>
#include <youtils/Logger.h>
#include <youtils/Main.h>
#include <youtils/TestMainHelper.h>

#include <backend/BackendTestSetup.h>

#include <volumedriver/test/MDSTestSetup.h>
#include <volumedriver/test/MetaDataStoreTestSetup.h>
#include <backend/TestWithBackendMainHelper.h>

namespace
{

namespace ara = arakoon;
namespace be = backend;
namespace fs = boost::filesystem;
namespace gt = ganeshatest;
namespace po = boost::program_options;
namespace vd = volumedriver;
namespace vdt = volumedrivertest;
namespace vfs = volumedriverfs;
namespace vfst = volumedriverfstest;
namespace yt = youtils;

class GaneshaTest
    : public backend::TestWithBackendMainHelper
{
public:
    GaneshaTest(int argc,
                   char** argv)
        : TestWithBackendMainHelper(argc, argv)
        , desc_("GaneshaTest Options")
        , arakoon_port_(0)
        , gdbserver_port_(0)
    {
        const std::string
            default_mdstore(boost::lexical_cast<std::string>(vdt::MetaDataStoreTestSetup::backend_type_));

        desc_.add_options()
            ("arakoon-binary-path",
             po::value<std::string>(&arakoon_path_)->default_value("/usr/bin/arakoon"),
             "path to arakoon binary")
            ("arakoon-port",
             po::value<uint16_t>(&arakoon_port_)->default_value(12345),
             "start of port range to use for arakoon")
            ("ganesha-binary-path",
             po::value<std::string>(&ganesha_path_)->required(),
             "path to ganesha.nfsd binary")
            ("volumedriver-fsal-path",
             po::value<std::string>(&fsal_path_)->required(),
             "path to FSAL plugins")
            ("backend-config-file,c",
             po::value<std::string>(&backend_config_file_),
             "backend config file")
            ("mds-port-base",
             po::value<uint16_t>(&vdt::MDSTestSetup::base_port_)->default_value(12356),
             "start of port range to use for MDS")
            ("metadata-store,M",
             po::value<std::string>(&mdstore_type_)->default_value(default_mdstore),
             "metadata store type: Arakoon/MDS/RocksDB/TCTBT")
            ("gdbserver-port,G",
             po::value<uint16_t>(&gdbserver_port_),
             "if specified, ganesha will be run under gdbserver listening on this port");
    }

    virtual void
    log_extra_help(std::ostream& os)
    {
        os << desc_ << std::endl;
        log_backend_setup_help(os);
        log_google_test_help(os);
    }

    virtual void
    setup_logging()
    {
        MainHelper::setup_logging();
    }

    virtual void
    parse_command_line_arguments()
    {
        TestWithBackendMainHelper::parse_command_line_arguments();


        parse_unparsed_options(desc_,
                               yt::AllowUnregisteredOptions::T,
                               vm_);
    }

    virtual int
    run()
    {
        ara::ArakoonTestSetup::setArakoonBinaryPath(arakoon_path_);
        ara::ArakoonTestSetup::setArakoonBasePort(arakoon_port_);

        vdt::MetaDataStoreTestSetup::backend_type_ =
            boost::lexical_cast<vd::MetaDataBackendType>(mdstore_type_);

        gt::Setup::set_ganesha_path(ganesha_path_);
        gt::Setup::set_fsal_path(fsal_path_);

        if (vm_.count("gdbserver-port"))
        {
            gt::Setup::set_gdbserver_port(gdbserver_port_);
        }

        //const auto ret = RUN_ALL_TESTS();

        //return ret;
        return TestWithBackendMainHelper::run();
    }

private:
    po::options_description desc_;
    std::string arakoon_path_;
    uint16_t arakoon_port_;
    std::string backend_config_file_;
    // std::string backend_type_;
    std::string ganesha_path_;
    std::string fsal_path_;
    std::string mdstore_type_;
    uint16_t gdbserver_port_;
};

}

MAIN(GaneshaTest);

// Local Variables: **
// mode: c++ **
// End: **
