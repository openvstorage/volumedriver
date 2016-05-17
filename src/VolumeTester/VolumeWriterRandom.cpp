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
#include <iostream>
#include "DeviceWriter.h"
#include <youtils/Logging.h>
#include <youtils/Logger.h>
#include "PatternStrategy.h"
#include "RandomStrategy.h"
#include "VTException.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <memory>
#include <youtils/Main.h>
#include <youtils/FileUtils.h>

namespace
{

namespace fs = boost::filesystem;
namespace po = boost::program_options;
enum class StrategyType
{
    Pattern,
    Random
};

std::istream&
operator>>(std::istream& strm,
           StrategyType& strat)
{
    std::string str;
    strm >> str;
    if(str == "pattern")
    {
        strat = StrategyType::Pattern;
    }
    else if(str == "random")
    {
        strat = StrategyType::Random;
    }
    else
    {
        strm.setstate(std::ios_base::failbit);
    }
    return strm;

}

class VolumeWriterRandom : public youtils::MainHelper
{
public:
    VolumeWriterRandom(int argc,
                       char** argv)
        : MainHelper(argc,
                     argv)
        , volumewriter_random_options_("VolumeWriterRandom options")
    {
        volumewriter_random_options_.add_options()
            ("device", po::value<fs::path>(&device_)->required(), "device to write to")
            ("reference-file", po::value<fs::path>(&reference_file_)->required(), "reference file to write to")
            ("strategy", po::value<StrategyType>(&strategy_)->required(), "strategy to use one of 'pattern' or 'random'")
            ("pattern", po::value<std::string>(&pattern_)->default_value(""), "Pattern to use for pattern strategy")
            ("rand", po::value<double>(&rand_)->required(), "Probability a block is written");

    }

    virtual void
    setup_logging()
    {
        MainHelper::setup_logging("volume_writer_random");
    }

    virtual void
    parse_command_line_arguments()
    {
        parse_unparsed_options(volumewriter_random_options_,
                               youtils::AllowUnregisteredOptions::T,
                               vm_);
    }

    virtual void
    log_extra_help(std::ostream& strm)
    {
        strm << volumewriter_random_options_;
    }

private:
    fs::path device_;
    fs::path reference_file_;
    StrategyType strategy_;
    std::string pattern_;
    double rand_;


    po::options_description volumewriter_random_options_;

    int run()
    {
        std::unique_ptr<Strategy> strat;

        switch(strategy_)
        {
        case StrategyType::Random:
            strat.reset(new RandomStrategy());
            break;
        case StrategyType::Pattern:
            strat.reset(new PatternStrategy());
            break;
        }

        if(!fs::exists(reference_file_))
        {
            LOG_FATAL("Reference file " << reference_file_ << " should exist, exiting");
            return 1;
        }

        DeviceWriter theWriter(device_.string(),
                               reference_file_.string(),
                               1,
                               0,
                               rand_,
                               *strat);
        theWriter();
        return 0;
    }
};

}

MAIN(VolumeWriterRandom)

// Local Variables: **
// End: **
