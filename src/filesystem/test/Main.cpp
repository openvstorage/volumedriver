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

#include "FileSystemEventTestSetup.h"
#include "FileSystemTestBase.h"

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

#include <filesystem/VirtualDiskFormatVmdk.h>
#include <backend/TestWithBackendMainHelper.h>

namespace ara = arakoon;
namespace be = backend;
namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace vd = volumedriver;
namespace vdt = volumedrivertest;
namespace vfs = volumedriverfs;
namespace vfst = volumedriverfstest;
namespace yt = youtils;

namespace
{

// TODO : SKIPPING THIS FOR NOW ONLY 1 FORMATSUPPORTED ANYWYA
enum class VmdkVDiskFormat
{
    Vmdk
};

const std::string vdisk_format_vmdk_name = "VMDK";


class FileSystemTest
    : public backend::TestWithBackendMainHelper
{
public:
    FileSystemTest(int argc,
                   char** argv)
        : TestWithBackendMainHelper(argc,
                                    argv)
        , desc_("FileSystemTest Options")
        , arakoon_port_(0)
        // , mdstore_options_(mdstore_alternatives())
        // , vdisk_format_options_(vdisk_format_alternatives())
    {
        const std::string
            default_mdstore(boost::lexical_cast<std::string>(vdt::MetaDataStoreTestSetup::backend_type_));

        desc_.add_options()
            ("arakoon-binary-path",
             po::value<decltype(arakoon_path_)>(&arakoon_path_)->default_value("/usr/bin/arakoon"),
             "path to arakoon binary")
            ("arakoon-port",
             po::value<decltype(arakoon_port_)>(&arakoon_port_)->default_value(12345),
             "start of port range to use for arakoon")
            ("volumedriverfs-binary-path",
             po::value<decltype(fs_binary_path_)>(&fs_binary_path_)->required(),
             "path to volumedriver_fs binary")
            ("arakoon-plugin-path",
             po::value<decltype(arakoon_plugins_)>(&arakoon_plugins_),
             "path to an arakoon plugin to use - can be specified multiple times")
            ("amqp-uri,A",
             po::value<decltype(amqp_uri_)>(&amqp_uri_),
             "AMQP URI")
            ("mds-port-base",
             po::value<uint16_t>(&vdt::MDSTestSetup::base_port_)->default_value(12356),
             "start of port range to use for MDS")
            ("metadata-store,M",
             po::value<decltype(mdstore_type_)>(&mdstore_type_)->default_value(default_mdstore),
             "metadata store type (see below for supported types)")
            ("address",
             po::value<decltype(vfst::FileSystemTestSetup::address_)>(&vfst::FileSystemTestSetup::address_)->default_value(vfst::FileSystemTestSetup::address_),
             "IP address to use")
            ("foc-transport",
             po::value<decltype(vfst::FileSystemTestSetup::failovercache_transport_)>(&vfst::FileSystemTestSetup::failovercache_transport_)->default_value(vfst::FileSystemTestSetup::failovercache_transport_),
             "FailOverCacheTransport to use (TCP|RSocket)")
            // ("vdisk-format,V",
            //  po::value<decltype(vdisk_format_)>(&vdisk_format_)->default_value(vdisk_format_vmdk_name),
            //  "vdisk format: VMDK")
            ;
    }

    virtual void
    log_extra_help(std::ostream& os)
    {
        // auto desc = desc_;
        // mdstore_options_->options_description(desc);
        // vdisk_format_options_->options_description(desc);
        youtils::print_doc<volumedrivertest::metadata_options_t>::go(os);
        os << desc_ << std::endl;
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

        {
            // yt::AlternativeOptionsAgain::mapped_type
            //     opt(mdstore_options_->at(mdstore_type_));
            auto opt = ::youtils::get_option<volumedriver::MetaDataBackendType,
                                             volumedrivertest::metadata_options_t>(mdstore_type_);

            parse_unparsed_options(opt->options_description(),
                                   yt::AllowUnregisteredOptions::T,
                                   vm_);
            opt->actions();

        }

        {
            // yt::AlternativeOptionsAgain::mapped_type
            //     opt(vdisk_format_options_->at(vdisk_format_));
            // parse_unparsed_options(opt->options_description(),
            //                        yt::AllowUnregisteredOptions::T,
            //                        vm_);
            std::unique_ptr<vfs::VirtualDiskFormat> fmt(new vfs::VirtualDiskFormatVmdk());
            vfst::FileSystemTestSetup::virtual_disk_format(std::move(fmt));

            //            (*opt)();
        }
    }

    virtual int
    run()
    {
        ara::ArakoonTestSetup::setArakoonBinaryPath(arakoon_path_);
        ara::ArakoonTestSetup::setArakoonBasePort(arakoon_port_);
        ara::ArakoonTestSetup::setArakoonPlugins(arakoon_plugins_);

        vfst::FileSystemTestBase::set_binary_path(fs_binary_path_);
        vfst::FileSystemEventTestSetup::set_amqp_uri(vfs::AmqpUri(amqp_uri_));
        // Great.. doesn't anybody do wrapper inheritance anymore
        // const auto ret = RUN_ALL_TESTS();

        return TestWithBackendMainHelper::run();
    }

private:
    po::options_description desc_;
    fs::path arakoon_path_;
    uint16_t arakoon_port_;
    std::vector<fs::path> arakoon_plugins_;
    fs::path fs_binary_path_;
    std::string amqp_uri_;

    std::string mdstore_type_;
    //    std::unique_ptr<yt::AlternativeOptionsAgain> mdstore_options_;

    std::string vdisk_format_;
    //    std::unique_ptr<yt::AlternativeOptionsAgain> vdisk_format_options_;
};

}

MAIN(FileSystemTest);

// Local Variables: **
// mode: c++ **
// End: **
