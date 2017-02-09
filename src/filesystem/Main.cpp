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

#include "FuseInterface.h"

#include <exception>
#include <future>
#include <iostream>
#include <cstdlib>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/tokenizer.hpp>

#include <youtils/BuildInfo.h>
#include <youtils/Catchers.h>
#include <youtils/ConfigFetcher.h>
#include <youtils/FileUrl.h>
#include <youtils/InitializedParam.h>
#include <youtils/Logger.h>
#include <youtils/Logging.h>
#include <youtils/Main.h>
#include <youtils/MainHelper.h>
#include <youtils/ScopeExit.h>
#include <youtils/SignalBlocker.h>
#include <youtils/SignalSet.h>
#include <youtils/System.h>
#include <youtils/Uri.h>
#include <youtils/VolumeDriverComponent.h>

namespace be = backend;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace po = boost::program_options;
namespace vfs = volumedriverfs;
namespace yt = youtils;

namespace
{

void
fuse_print_helper(const std::string& argv0,
                  const std::string& opt)
{
    std::vector<char*> argv(2);

    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         for (char* str : argv)
                                         {
                                             free(str);
                                         }
                                     }));

    argv[0] = strdup(argv0.c_str());
    argv[1] = strdup(opt.c_str());
    fuse_operations dummy;
    bzero(&dummy, sizeof(dummy));
    fuse_main(argv.size(), &argv[0], &dummy, NULL);
}

void
print_fuse_help(const std::string& argv0)
{
    fuse_print_helper(argv0, "-ho");
}

void
print_config_help_markdown()
{
    std::cout << "| component | key | default value | dynamically reconfigurable | remarks |" << std::endl;
    std::cout << "| --- | --- | --- | --- | --- |" << std::endl;
    for (const auto& info : initialized_params::ParameterInfo::get_parameter_info_list())
    {
        if (T(info->document_))
        {
            std::cout << "| " << info->section_name;
            std::cout << " | " << info->name;
            std::cout << " | " << (info->has_default ? (std::string("\"") +  info->def_  + "\"") : "---");
            std::cout << " | " << (info->can_be_reset ? "yes" : "no");
            std::cout << " | " << info->doc_string;
            std::cout << " |" << std::endl;
        }
    }
}

void
print_config_help()
{
    for (const auto& info : ip::ParameterInfo::get_parameter_info_list())
    {
        if (T(info->document_))
        {
            std::cout << "* " << info->section_name << "/" << info->name << std::endl;

            if (info->has_default)
            {
                std::cout << "** " << "default: " << info->def_ << std::endl;
            }
            else
            {
                std::cout << "** " << "required parameter " << std::endl;
            }

            if (info->dimensioned_value)
            {
                std::cout << "** Dimensioned Literal (\"234MiB\")" << std::endl;
            }
            else
            {
                std::cout << "** Normal Literal " << std::endl;
            }

            if (info->can_be_reset)
            {
                std::cout << "** " << "Parameter can be reconfigured dynamically" << std::endl;
            }
            else
            {
                std::cout << "** " << "Parameter cannot be reconfigured dynamically" << std::endl;
            }
        }
    }
}

const char* config_help_flag = "config-help";
const char* config_help_markdown_flag = "config-help-markdown";
const char* dont_restart_volumes_flag = "dont-restart-volumes";

class FileSystemMain
    : public yt::MainHelper
{
public:
    FileSystemMain(int argc,
                   char** argv)
        : yt::MainHelper(argc, argv)
        , opts_("FileSystem options")
    {
        opts_.add_options()
            ("config-file,C",
             po::value<yt::MaybeUri>(&config_location_),
             "volumedriver (json) config file / etcd URL (deprecated, use --config instead!)")
            ("config",
             po::value<yt::MaybeUri>(&config_location_),
             "volumedriver (json) config file / etcd URL")
            (config_help_flag,
             "print configuration documentation and exit")
            (config_help_markdown_flag,
             "print configuration documentation in markdown format and exit")
            (dont_restart_volumes_flag,
             "don't restart local volumes")
            ("lock-file,L",
             po::value<std::string>(&lock_file_),
             "a lock file used for advisory locking to prevent concurrently starting the same instance - the config-file is used if this is not specified")
            ("mountpoint,M",
             po::value<std::string>(&mountpoint_),
             "path to mountpoint");
    }

    virtual void
    parse_command_line_arguments()
    {
        po::parsed_options
            parsed(parse_unparsed_options(opts_,
                                          yt::AllowUnregisteredOptions::T,
                                          vm_));

        // XXX:
        // pretend there were not unparsed_options_ left and pass them behind
        // MainHelper's back to the FileSystem/FUSE
        unparsed_ = po::collect_unrecognized(parsed.options,
                                             boost::program_options::include_positional);
        unparsed_options_.clear();
    }

    virtual int
    run()
    {
        if (vm_.count(config_help_flag))
        {
            print_config_help();
            return 0;
        }

        if (vm_.count(config_help_markdown_flag))
        {
            print_config_help_markdown();
            return 0;
        }

        if (not config_location_)
        {
            std::cerr << "No config location specified" << std::endl;
            return 1;
        }

        if (mountpoint_.empty())
        {
            std::cerr << "No mountpoint specified" << std::endl;
            return 1;
        }

        umask(0);

        const vfs::RestartVolumes restart_volumes =
            (vm_.count(dont_restart_volumes_flag) == 0) ?
            vfs::RestartVolumes::T :
            vfs::RestartVolumes::F;

        const bool lock_config_file = lock_file_.empty();
        if (lock_config_file and not yt::FileUrl::is_one(*config_location_))
        {
            std::cerr << "No lock file specified / config is not read from a local file" << std::endl;
            return 1;
        }

        const fs::path lock_path(lock_config_file ?
                                 config_location_->path() :
                                 lock_file_);

        // These are unblocked again and "handled" by FileSystem::operator(). We do
        // however want them to be blocked while the constructor / destructor is running.
        // This of course points to making the blocker a member of FileSystem but that
        // will get in the way of testing as there we want to be able to Ctrl-C out of
        // a tester.
        const yt::SignalSet block{ SIGHUP,
                // SIGINT, // screws up gdb!
                SIGPIPE,
                SIGTERM };

        const yt::SignalBlocker blocker(block);

        yt::FileDescriptor fd(lock_path,
                              yt::FDMode::Write,
                              lock_config_file ?
                              CreateIfNecessary::F :
                              CreateIfNecessary::T);

        std::unique_lock<decltype(fd)> u(fd,
                                         std::try_to_lock);
        if (not u)
        {
            std::cerr << "Failed to lock " << lock_path <<
                " - is another instance already using it?" << std::endl;
            return 1;
        }

        std::unique_ptr<yt::ConfigFetcher> config_fetcher(yt::ConfigFetcher::create(*config_location_));
        const bpt::ptree pt((*config_fetcher)(VerifyConfig::T));

        vfs::FuseInterface(pt,
                           RegisterComponent::T,
                           restart_volumes)(mountpoint_,
                                            unparsed_);

        return 0;
    }

    virtual void
    setup_logging()
    {
        MainHelper::setup_logging("volumedriverfs");
    }

    virtual void
    log_extra_help(std::ostream& os)
    {
        os << opts_ << std::endl;

        auto fuse_help(yt::System::with_captured_c_fstream(stderr,
                                                           [&]()
                                                           {
                                                               print_fuse_help(executable_name_);
                                                           }));
        os << fuse_help;
    }

private:
    DECLARE_LOGGER("VolumeDriverFS");

    po::options_description opts_;
    yt::MaybeUri config_location_;
    std::string lock_file_;
    std::string mountpoint_;

    std::vector<std::string> unparsed_;
};

}

MAIN(FileSystemMain)
