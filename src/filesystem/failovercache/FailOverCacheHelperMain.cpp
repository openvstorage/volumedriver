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

#include <unistd.h>

#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <youtils/Assert.h>
#include <youtils/Logging.h>

#include <../ConfigHelper.h>

namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace vfs = volumedriverfs;

// Not using MainHelper as it only has 2 args itself and routes the rest through.
int
main(int argc,
     char** argv)
{
    fs::path config_file;
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
        ("config-file,C",
         po::value<fs::path>(&config_file)->required(),
         "volumedriverfs (json) config file");

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

    // if(config_file.empty())
    // {
    //     std::cerr << desc;

    //     std::cerr("You should pass a valid configuration file as option");

    // }
    //
    // LOG_INFO("Parsing the property tree in " << config_file_);

    bpt::ptree pt;
    bpt::json_parser::read_json(config_file.string(), pt);

    const vfs::ConfigHelper argument_helper(pt);

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
