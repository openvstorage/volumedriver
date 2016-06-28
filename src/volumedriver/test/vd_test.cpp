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

#include "ExGTest.h"
#include "DtlTestSetup.h"
#include "MDSTestSetup.h"
#include "MetaDataStoreTestSetup.h"
#include "VolManagerTestSetup.h"

#include "../DebugPrint.h"
#include "../MetaDataBackendConfig.h"

#include <string>
#include <iostream>
#include <sstream>
#include <cassert>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/ptr_container/ptr_map.hpp>

#include <youtils/ArakoonTestSetup.h>
#include <youtils/BuildInfo.h>
#include <youtils/Logger.h>
#include <youtils/Main.h>

#include <backend/BackendTestSetup.h>
#include <backend/TestWithBackendMainHelper.h>

namespace ara = arakoon;
namespace mds = metadata_server;
namespace vd = volumedriver;
namespace vdt = volumedrivertest;
namespace yt = youtils;
namespace be = backend;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

class VolumeDriverTest
    : public backend::TestWithBackendMainHelper
{
public:
    VolumeDriverTest(int argc,
                     char** argv)
        : TestWithBackendMainHelper(argc,
                                    argv)
        , desc_("Common Options")
    {
        const std::string
            default_mdstore(boost::lexical_cast<std::string>(vdt::MetaDataStoreTestSetup::backend_type_));

        desc_.add_options()
            ("arakoon-binary-path",
             po::value<std::string>(&arakoon_binary_path_)->default_value("/usr/bin/arakoon"),
             "path to arakoon binary")
            ("arakoon-port-base",
             po::value<uint16_t>(&arakoon_port_base_)->default_value(12345),
             "arakoon port base")
            ("foc-address",
             po::value<std::string>(&vdt::DtlTestSetup::addr_)->default_value(vdt::DtlTestSetup::addr_),
             "address to bind the failovercache to")
            ("foc-port-base,F",
             po::value<uint16_t>(&vdt::DtlTestSetup::port_base_)->default_value(vdt::DtlTestSetup::port_base_),
             "FailOverCache port base")
            ("foc-transport",
             po::value<decltype(vdt::DtlTestSetup::transport_)>(&vdt::DtlTestSetup::transport_)->default_value(vdt::DtlTestSetup::transport_),
             "FailOverCache transport (TCP|RSocket)")
            ("mds-port-base",
             po::value<uint16_t>(&vdt::MDSTestSetup::base_port_)->default_value(vdt::MDSTestSetup::base_port_),
             "start of port range to use for MDS")
            ("metadata-store,M",
             po::value<std::string>(&mdstore_type_)->default_value(default_mdstore),
             "metadata store to use (see below for supported types)")
            ;
    }

    virtual void
    log_extra_help(std::ostream& strm)
    {
        // needs to be replaced by a call on gtest that will log instead of write to std::out
        // Only called if the --help flag is set *and* we're gonna exit!
        // mdstore_options_->options_description(desc_);
        strm << desc_;
        youtils::print_doc<volumedrivertest::metadata_options_t>::go(strm);
        // should log to the stream but logs to std::cout...
        log_backend_setup_help(strm);

        log_google_test_help(strm);
    }

    virtual void
    setup_logging()
    {
        MainHelper::setup_logging("volumedriver_test");
    }

    virtual void
    parse_command_line_arguments()
    {
        TestWithBackendMainHelper::parse_command_line_arguments();

        parse_unparsed_options(desc_,
                               yt::AllowUnregisteredOptions::T,
                               vm_);

        parse_mdstore_config_();

        ara::ArakoonTestSetup::setArakoonBinaryPath(arakoon_binary_path_);
        ara::ArakoonTestSetup::setArakoonBasePort(arakoon_port_base_);
    }

    po::options_description desc_;
    std::string arakoon_binary_path_;
    uint16_t arakoon_port_base_;
    std::string mdstore_type_;

private:
    void
    parse_mdstore_config_()
    {
        po::variables_map vm;
        auto opt = ::youtils::get_option<volumedriver::MetaDataBackendType,
                                         volumedrivertest::metadata_options_t>(mdstore_type_);

        parse_unparsed_options(opt->options_description(),
                               yt::AllowUnregisteredOptions::T,
                               vm);

        opt->actions();
    }
};

MAIN(VolumeDriverTest)

// Local Variables: **
// mode: c++ **
// End: **
