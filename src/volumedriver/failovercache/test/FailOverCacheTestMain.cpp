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

#include "FailOverCacheTestMain.h"

namespace po = boost::program_options;
namespace vd = volumedriver;

std::string
FailOverCacheTestMain::host_("127.0.0.1");

uint16_t
FailOverCacheTestMain::port_ = 23096;

vd::FailOverCacheTransport
FailOverCacheTestMain::transport_(vd::FailOverCacheTransport::TCP);

std::unique_ptr<backend::Namespace>
FailOverCacheTestMain::ns_;

FailOverCacheTestMain::FailOverCacheTestMain(int argc,
                                             char** argv)
    : TestMainHelper(argc,
                     argv)
{
    normal_options_.add_options()
        ("host",
         po::value<std::string>(&host_)->default_value(host_),
         "host of the failovercache server")
        ("port",
         po::value<uint16_t>(&port_)->default_value(port_),
         "port of the failovercache server")
        ("transport",
         po::value<vd::FailOverCacheTransport>(&transport_)->default_value(transport_),
         "transport type of the failovercache server (TCP|RSocket)")
        ("namespace",
         po::value<std::string>(&ns_temp_)->default_value("namespace"),
         "namespace to use for testing");
};

void
FailOverCacheTestMain::setup_logging()
{
    MainHelper::setup_logging("dtl_test");
}

void
FailOverCacheTestMain::log_extra_help(std::ostream& os)
{
    os << normal_options_ << std::endl;
    log_google_test_help(os);
}

void
FailOverCacheTestMain::parse_command_line_arguments()
{
    init_google_test();
    parse_unparsed_options(normal_options_,
                           youtils::AllowUnregisteredOptions::T,
                           vm_);
    ns_.reset(new backend::Namespace(ns_temp_));
}

MAIN(FailOverCacheTestMain);
