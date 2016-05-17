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

#include <unistd.h>

#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <youtils/Assert.h>
#include <youtils/Logging.h>

#include <../ConfigFetcher.h>
#include <../ConfigHelper.h>

namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace vfs = volumedriverfs;

// Not using MainHelper as it only has 2 args itself and routes the rest through.
int
main(int argc,
     char** argv)
{
    std::string config_location;
    fs::path failovercache_executable;
    std::vector<std::string> unparsed_options;
    po::options_description desc;
    po::command_line_parser p = po::command_line_parser(argc, argv);
    p.style(po::command_line_style::default_style ^
            po::command_line_style::allow_guessing);
    p.allow_unregistered();
    boost::program_options::variables_map vm;

    desc.add_options()
        ("failovercache-executable",
         po::value<fs::path>(&failovercache_executable)->default_value("failovercache"),
         "path to the failovercache executable")
        ("config",
         po::value<std::string>(&config_location),
         "volumedriverfs (json) config file / etcd URL")
        ("config-file,C",
         po::value<std::string>(&config_location),
         "volumedriverfs (json) config file (deprecated, use --config instead!)");

    {
        po::options_description help_options;
        help_options.add_options()
            ("help", "get some help");

        p.options(help_options);
        po::parsed_options parsed = p.run();
        po::store(parsed, vm);
        po::notify(vm);
        if(vm.count("help"))
        {
            std::clog << "Helper program to start the failover cache from a volumedriverfs config file" << std::endl;
            std::clog << "For help with failovercache itself, run that executable with the --help flag" << std::endl;
            std::clog << desc;
            exit(0);
        }
    }

    {
        po::command_line_parser p = po::command_line_parser(argc, argv);
        p.style(po::command_line_style::default_style ^
                po::command_line_style::allow_guessing);
        p.options(desc);
        p.allow_unregistered();
        po::parsed_options parsed = p.run();
        po::store(parsed, vm);
        po::notify(vm);

        unparsed_options = po::collect_unrecognized(parsed.options,
                                                    po::include_positional);
    }

    // if(config_location.empty())
    // {
    //     std::cerr << desc;

    //     std::cerr("You should pass a valid configuration file as option");

    // }
    //
    // LOG_INFO("Parsing the property tree in " << config_location_);

    if (config_location.empty())
    {
        std::cerr << "No config location specified" << std::endl;
        return 1;
    }

    vfs::ConfigFetcher config_fetcher(config_location);
    const vfs::ConfigHelper argument_helper(config_fetcher(VerifyConfig::F));

    // LOG_INFO("Finished parsing property tree");

    const std::string
        path_for_failover_cache(argument_helper.failover_cache_path().string());
    const std::string
        addr(argument_helper.localnode_config().host);
    const auto
        port(boost::lexical_cast<std::string>(argument_helper.failover_cache_port()));
    const auto
        transport(boost::lexical_cast<std::string>(argument_helper.failover_cache_transport()));

    const std::string arg0 = failovercache_executable.filename().string();
    const std::string arg1 = "--address=" + addr;
    const std::string arg2 = "--path=" + path_for_failover_cache;
    const std::string arg3 = "--port=" + port;
    const std::string arg4 = "--transport=" + transport;
    // LOG_INFO("Execing " << arg0 << " " << arg1 << " " << arg2);

    std::vector<const char*> new_args;
    new_args.push_back(arg0.c_str());
    new_args.push_back(arg1.c_str());
    new_args.push_back(arg2.c_str());
    new_args.push_back(arg3.c_str());
    new_args.push_back(arg4.c_str());

    for(const auto& i : unparsed_options)
    {
        new_args.push_back(i.c_str());
    }
    new_args.push_back(nullptr);

    int retval = execvp(failovercache_executable.string().c_str(),
                        (char* const*)new_args.data());

    if(retval == -1)
    {
        std::cerr << "Could not start failover cache executable: " <<
            failovercache_executable << std::endl;
        std::cerr << "Args: " << std::endl;

        for(unsigned i = 0; i < (new_args.size() - 1); ++i)
        {
            std::cerr << "arg[" << i << "]: " << new_args[i] << std::endl;
        }

        return 1;
    }
}
