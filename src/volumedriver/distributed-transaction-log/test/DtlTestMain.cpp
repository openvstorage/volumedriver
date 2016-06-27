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

#include "DtlTestMain.h"

namespace po = boost::program_options;
namespace vd = volumedriver;

std::string
DtlTestMain::host_("127.0.0.1");

uint16_t
DtlTestMain::port_ = 23096;

vd::DtlTransport
DtlTestMain::transport_(vd::DtlTransport::TCP);

std::unique_ptr<backend::Namespace>
DtlTestMain::ns_;

DtlTestMain::DtlTestMain(int argc,
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
         po::value<vd::DtlTransport>(&transport_)->default_value(transport_),
         "transport type of the failovercache server (TCP|RSocket)")
        ("namespace",
         po::value<std::string>(&ns_temp_)->default_value("namespace"),
         "namespace to use for testing");
};

void
DtlTestMain::setup_logging()
{
    MainHelper::setup_logging("dtl_test");
}

void
DtlTestMain::log_extra_help(std::ostream& os)
{
    os << normal_options_ << std::endl;
    log_google_test_help(os);
}

void
DtlTestMain::parse_command_line_arguments()
{
    init_google_test();
    parse_unparsed_options(normal_options_,
                           youtils::AllowUnregisteredOptions::T,
                           vm_);
    ns_.reset(new backend::Namespace(ns_temp_));
}

MAIN(DtlTestMain);
