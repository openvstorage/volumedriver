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

#ifndef MAIN_HELPER_H_
#define MAIN_HELPER_H_

#include "Assert.h"
#include "BuildInfoString.h"
#include "IOException.h"
#include "Logger.h"
#include "Logging.h"
#include "OptionValidators.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>

namespace youtils
{

BOOLEAN_ENUM(ExcludeExecutableName);
BOOLEAN_ENUM(AllowUnregisteredOptions);
BOOLEAN_ENUM(MakeDefaultLocalBackendIfNotSet);


class MainHelper
{
    friend int main(int argc,
                    char** argv);

protected:
    typedef std::pair<std::string, std::vector<std::string> > constructor_type;

    static constructor_type
    fromArgcArgv(int argc,
                 char** argv);

    MainHelper(const constructor_type&);

    MainHelper(int argc,
               char** argv);

    virtual ~MainHelper() = default;

public:
    int
    operator()();

    static boost::property_tree::ptree
    getBackendPTree(MakeDefaultLocalBackendIfNotSet = MakeDefaultLocalBackendIfNotSet::T);

private:
    virtual void
    on_log_rotate()
    { }
protected:
#ifndef NO_MAIN_HELPER_CORBA_SUPPORT
    void
    log_corba_server_help(std::ostream&);

    void
    log_corba_client_help(std::ostream&);

    void
    parse_corba_client_args();

    void
    parse_corba_server_args();
#endif
    virtual void
    parse_command_line_arguments() = 0;

    virtual void
    log_extra_help(std::ostream& strm) = 0;

    virtual void
    at_exit();

    void
    print_version_data();

    virtual int
    run() = 0;

    void
    parse_standard_options();

    virtual void
    check_no_unrecognized();

protected:
    boost::program_options::variables_map vm_;
    std::vector<std::string> unparsed_options_;

public:
    static MaybeUri backend_config_;

private:
    std::vector<std::string> args_;

    boost::program_options::options_description logging_options_;
    boost::program_options::options_description general_options_;
    boost::program_options::options_description standard_options_;
    boost::program_options::options_description backend_options_;

    ExistingFile backend_config_file_;

protected:
    const std::string executable_name_;
    SeverityLoggerWithName main_logger_;

protected:

    struct ArgcArgv
    {
        ArgcArgv(const char* executable_name,
                 const std::vector<std::string> args)
            : argc_(args.size() + 1)
            , argv_(0)
            , orig_size_(args.size() + 2)
        {
            // might leak if the constructor throws -- but we probably have other worries then.

            argv_ = new char* [orig_size_];
            my_orig_mem = new char*[orig_size_];

            unsigned index = 0;

            unsigned len = strlen(executable_name);
            char* c = new char[ len + 1];
            strncpy(c, executable_name, len +1);
            my_orig_mem[index] = c;
            argv_[index++] = c;

            for(const std::string& str : args)
            {
                len = str.size();
                char* c = new char[len + 1];
                strncpy(c, str.c_str(), len + 1);
                my_orig_mem[index] = c;
                argv_[index++] = c;
            }
            argv_[index++] = nullptr;
            VERIFY(index == orig_size_);
        }

        DECLARE_LOGGER("ArgcArgv");

        ~ArgcArgv()
        {
            for(unsigned i = 0; i < (orig_size_ - 1); ++i)
            {
                delete[] my_orig_mem[i];
            }
            delete[] argv_;
            delete[] my_orig_mem;
        }

        int*
        argc()
        {
            return &argc_;
        }

        char** argv()
        {
            return argv_;
        }

        int argc_;
        char** argv_;
        const unsigned orig_size_;
        char** my_orig_mem;

    };

    virtual void
    setup_logging() = 0;

    void
    setup_logging(const std::string&);

    inline SeverityLoggerWithName&
    getLogger__()
    {
        return main_logger_;
    }

    void
    unparsed_options(const boost::program_options::parsed_options& parsed_options,
                     ExcludeExecutableName = ExcludeExecutableName::F);

    void
    unparsed_options(int argc,
                     char** argv);

    const std::vector<std::string>&
    unparsed_options() const;

    boost::program_options::parsed_options
    parse_unparsed_options(const boost::program_options::options_description& opts,
                           AllowUnregisteredOptions allow_unregistered,
                           boost::program_options::variables_map& vm);

    std::vector<std::string> logsinks_;
    std::string logfile_;
    Severity loglevel_;
    LogRotation log_rotation_;

    std::vector<std::string> filters;
};
}

#endif // MAIN_HELPER_H_


// Local Variables: **
// mode: c++ **
// End: **
