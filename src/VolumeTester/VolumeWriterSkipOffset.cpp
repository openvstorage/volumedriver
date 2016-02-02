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

class VolumeWriterSkipOffset : public youtils::MainHelper
{
public:
    VolumeWriterSkipOffset(int argc,
                           char** argv)
        : MainHelper(argc,
                     argv)
        , volumewriter_skipoffset_options_("VolumeWriterSkipOffset options")
    {
        volumewriter_skipoffset_options_.add_options()
            ("device", po::value<fs::path>(&device_)->required(), "device to write to")
            ("reference-file", po::value<fs::path>(&reference_file_)->required(), "reference file to write to")
            ("strategy", po::value<StrategyType>(&strategy_)->required(), "strategy to use one of 'pattern' or 'random'")
            ("pattern", po::value<std::string>(&pattern_)->default_value(""), "Pattern to use for pattern strategy")
            ("firstblock", po::value<uint64_t>(&firstblock_)->required(), "First block to write")
            ("step", po::value<uint64_t>(&step_)->required(), "interval at which to write");
    }


    virtual void
    setup_logging()
    {
        MainHelper::setup_logging("volume_writer_skip_offset");
    }

    virtual void
    parse_command_line_arguments()
    {
        parse_unparsed_options(volumewriter_skipoffset_options_,
                               youtils::AllowUnregisteredOptions::T,
                               vm_);
    }

    virtual void
    log_extra_help(std::ostream& strm)
    {
        strm << volumewriter_skipoffset_options_;
    }

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

        if(fs::exists(reference_file_))
        {
            LOG_FATAL("Reference file " << reference_file_ << " should not exist, exiting");
            return 1;
        }


        DeviceWriter theWriter(device_.string(),
                               reference_file_.string(),
                               step_,
                               firstblock_,
                               2.0,
                               *strat);
        theWriter();
        return 0;
    }
private:

    fs::path device_;
    fs::path reference_file_;
    StrategyType strategy_;
    std::string pattern_;
    uint64_t firstblock_;
    uint64_t step_;
    po::options_description volumewriter_skipoffset_options_;
};

}

MAIN(VolumeWriterSkipOffset)

// Local Variables: **
// mode: c++ **
// End: **
