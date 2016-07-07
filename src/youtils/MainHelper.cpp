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

#include "ConfigLocation.h"
#include "FileUtils.h"
#include "Logging.h"
#include "MainHelper.h"
#include "ProtobufLogger.h"
#include "VolumeDriverComponent.h"

#include <iostream>

#include <sys/resource.h>

namespace youtils
{

namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace po = boost::program_options;

MaybeConfigLocation
MainHelper::backend_config_;

MainHelper::MainHelper(const constructor_type& args)
    : unparsed_options_(args.second)
    , logging_options_("Logging Options")
    , general_options_("General Options")
    , backend_options_("Backend Options")
    , executable_name_(args.first)
    , main_logger_(executable_name_)
{
    logging_options_.add_options()
        ("logsink",
         po::value<decltype(logsinks_)>(&logsinks_)->composing(),
         "Where to write the log messages, can be 'console:' or '/path/to/logfile'")
        ("logfile",
         po::value<decltype(logfile_)>(&logfile_),
         "Path to logfile (deprecated, use logsink instead!)")
        ("loglevel",
         po::value<Severity>(&loglevel_)->default_value(Severity::info),
         "Log level: [trace|debug|periodic|info|warning|error|fatal|notify]")
        ("disable-logging",
         "Whether to disable the logging")
        ("logrotation",
         "whether to rotate log files at midnight")
        ("logfilter",
         po::value<std::vector<std::string>>(&filters)->composing(),
         "a log filter, each filter is of the form 'logger_name loggerlevel', multiple filters can be added");

    general_options_.add_options()
        ("help,h", "produce help message on stdout and exit")
        ("version,v", "print version and exit");

    backend_options_.add_options()
        ("backend-config-file",
         po::value<ExistingFile>(&backend_config_file_),
         "backend config file, a json file that holds the backend configuration (DEPRECATED, use 'backend-config' instead!)")
        ("backend-config",
         po::value<decltype(backend_config_)>(&backend_config_),
         "backend config (file, etcd url, ...)");

    standard_options_.add(logging_options_)
        .add(general_options_)
        .add(backend_options_);
}

MainHelper::constructor_type
MainHelper::fromArgcArgv(int argc,
                         char** argv)
{
    ASSERT(argc > 0);
    std::string executable_name(argv[0]);

    std::vector<std::string> args;
    for(int i = 1; i < argc; ++i)
    {
        args.emplace_back(argv[i]);
    }
    return std::make_pair(executable_name, args);
}

int
MainHelper::operator()()
{
    parse_standard_options();
    int res = 1;

    try
    {
        setup_logging();
        parse_command_line_arguments();
        check_no_unrecognized();
        print_version_data();

        res = run();
    }
    catch (std::exception& e)
    {
        LOG_FATAL("caught exception: " << e.what());
    }
    catch (...)
    {
        LOG_FATAL("caught unknown exception");
    }

    at_exit();

    google::protobuf::ShutdownProtobufLibrary();

    return res;

}

MainHelper::MainHelper(int argc,
                       char** argv)
    : MainHelper(fromArgcArgv(argc, argv))
{
    ASSERT(argc > 0);
}

void
MainHelper::setup_logging(const std::string& progname)
{
    // We should try to have *no* logging before this is called!!!
    log_rotation_ = vm_.count("logrotation") ? LogRotation::T : LogRotation::F;

    if(vm_.count("disable-logging"))
    {
        Logger::disableLogging();
    }
    else
    {
        if (not logfile_.empty())
        {
            logsinks_.push_back(logfile_);
        }

        if (logsinks_.empty())
        {
            logsinks_.push_back(Logger::console_sink_name());
        }

        Logger::setupLogging(progname,
                             logsinks_,
                             loglevel_,
                             log_rotation_);

        for(std::string& str : filters)
        {
            std::stringstream ss(str);
            std::string filtername;
            Severity sev;

            ss >> filtername;
            ss >> sev;
            if(ss)
            {
                LOG_NOTIFY("Adding filter <" << filtername << ", " << sev << ">");
                Logger::add_filter(filtername,
                                   sev);
            }
        }
    }
    // Seems strange to be non configuratble
    ProtobufLogger::setup();
}

namespace
{

po::parsed_options
parse_helper(const std::vector<std::string> args,
             const po::options_description& opts,
             AllowUnregisteredOptions allow_unregistered,
             po::variables_map& vm)
{
    po::command_line_parser p = po::command_line_parser(args);
    p.style(po::command_line_style::default_style ^
            po::command_line_style::allow_guessing);
    p.options(opts);

    if (allow_unregistered == AllowUnregisteredOptions::T)
    {
        p.allow_unregistered();
    }

    po::parsed_options parsed = p.run();
    po::store(parsed, vm);
    po::notify(vm);

    return parsed;
}

}

void
MainHelper::parse_standard_options()
{
    po::parsed_options p(parse_helper(unparsed_options(),
                                      standard_options_,
                                      AllowUnregisteredOptions::T,
                                      vm_));

    unparsed_options(p,
                     ExcludeExecutableName::T);

    if(vm_.count("version"))
    {
        std::cout << buildInfoString();
        exit(0);
    }

    if(vm_.count("help"))
    {
        std::cout << standard_options_ << std::endl;
        log_extra_help(std::cout);
        exit(0);
    }

    if (not backend_config_ and backend_config_file_)
    {
        backend_config_ = ConfigLocation(backend_config_file_->string());
    }
}


void
MainHelper::unparsed_options(int argc,
                             char** argv)
{
    VERIFY(argc > 0);
    VERIFY(std::string(argv[0]) == std::string(executable_name_));

    unparsed_options_.clear();

    for(int i = 1; i < argc; ++i)
    {
        unparsed_options_.emplace_back(argv[i]);
    }
}

const std::vector<std::string>&
MainHelper::unparsed_options() const
{
    return unparsed_options_;
}

void
parse_google_test_commandline()
{}

void
MainHelper::unparsed_options(const po::parsed_options& parsed_options,
                             ExcludeExecutableName)
{
    unparsed_options_ =
        po::collect_unrecognized(parsed_options.options,
                                 po::include_positional);
}

void
MainHelper::check_no_unrecognized()
{
    if(not unparsed_options_.empty())
    {
        LOG_FATAL("Unrecognized options:");
        for(const std::string& option : unparsed_options_)
        {
            LOG_FATAL(option);
        }
        throw fungi::IOException("Unrecognized options");
    }
}


void
MainHelper::at_exit()
{
    struct rusage ru;
    if(getrusage(RUSAGE_SELF, &ru) == 0)
    {

        LOG_NOTIFY("Time spent executing user instructions (seconds): "
                   << ru.ru_utime.tv_sec << "." << ru.ru_utime.tv_usec << std::endl
                   << "Time spent in operating system code (seconds): "
                   << ru.ru_stime.tv_sec << "." << ru.ru_stime.tv_usec << std::endl
                   << "The maximum resident set size used, in kilobytes. "
                   <<  ru.ru_maxrss << std::endl
                   << "The number of page faults which were serviced without requiring any I/O: "
                   << ru.ru_minflt << std::endl
                   << "The number of page faults which were serviced by doing I/O: "
                   << ru.ru_majflt << std::endl
                   << "The number of times PROCESSES was swapped entirely out of main memory: "
                   << ru.ru_nswap << std::endl
                   << "The number of times the file system had to read from the disk: "
                   << ru.ru_inblock << std::endl
                   << "The number of times the file system had to write to the disk: "
                   << ru.ru_oublock << std::endl
                   << "The number of signals received: "
                   << ru.ru_nsignals << std::endl
                   << "The number of times PROCESSES voluntarily invoked a context switch: "
                   << ru.ru_nvcsw << std::endl
                   << "The number of times an involuntary context switch took place: "
                   << ru.ru_nivcsw << std::endl);
    }
}

void
MainHelper::print_version_data()
{
    LOG_NOTIFY(buildInfoString());

    LOG_NOTIFY("Command Line Options");

    for(const std::string& arg : args_)
    {
        LOG_NOTIFY(arg);
    }
}

po::parsed_options
MainHelper::parse_unparsed_options(const po::options_description& opts,
                                   AllowUnregisteredOptions allow_unregistered,
                                   po::variables_map& vm)
{
   po::parsed_options p(parse_helper(unparsed_options(),
                                      opts,
                                      allow_unregistered,
                                      vm));
    unparsed_options(p);
    return p;
}


void
MainHelper::log_corba_server_help(std::ostream&)
{}


void
MainHelper::log_corba_client_help(std::ostream&)
{}


void
MainHelper::parse_corba_client_args()
{}

void
MainHelper::parse_corba_server_args()
{}


}

// Local Variables: **
// mode: c++ **
// End: **
