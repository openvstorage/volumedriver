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

#include <string>
#include <assert.h>

#include <unistd.h>
#include <iostream>
#include <stdlib.h>

#include <boost/program_options.hpp>

#include <youtils/Logger.h>
#include <youtils/Logging.h>
#include <youtils/Main.h>

#include "RandomDeviceReader.h"
#include "DeviceReader.h"
#include "VTException.h"

namespace
{

namespace po = boost::program_options;

//returns codes
//0 -> files/device are the same
//1 -> files/device are different
//2 -> exception occurred (probably IO-error)
/* This is the eventual way these things should be done: check type first *then* check the
   command line args for that type
enum class VolumeReaderType
{
    full_sweep,
    random_read
};

template <VolumeReaderType>
struct VolumeReaderTypeString
{};

template<>
VolumeReaderTypeString<VolumeReaderType::full_sweep>
{
    static const char* name = "full-sweep";
}

template<>
VolumeReaderTypeString<VolumeReaderType::random_read>
{
    static const char* name = "random-read";
}



    // For parsing command line options
std::istream&
operator>>(std::istream& istr,
           VolumeReaderType type)
{
    std::string enum_str;
    istr >> enum_str;
    if(enum_str == VolumeReaderTypeString<full_sweep>::name)
    {
        type = VolumeReaderType::full_sweep;
    }
    else if (enum_str == VolumeReaderTypeString<random_read>::name)
    {
        type = VolumeReaderType::random_read;
    }
    else
    {
        istr.setstate(std::ios_base::failbit);
    }
    return strm;
}

*/

class VolumeReader : public youtils::MainHelper
{
public:
    VolumeReader(int argc,
                 char** argv)
        : MainHelper(argc,
                     argv)
        , full_sweep(false)
        , random_read(false)
        , full_sweep_options_("options for a full sweep")
        , random_read_options_("options for a random read")
    {
        full_sweep_options_.add_options()
            ("full-sweep", "do a full sweep of the device")
            ("read-chance", po::value<double>(&read_chance)->default_value(1.0), "change to read an lba")
            ("device-file", po::value<std::string>(&device_file)->required(),"device file to read")
            ("reference-file", po::value<std::string>(&reference_file)->default_value(""),"reference file to read");

        random_read_options_.add_options()
            ("random-read", "do random read of the device")
            ("num-reads", po::value<uint64_t>(&num_reads)->required(), "number of reads to do")
            ("device-file", po::value<std::string>(&device_file)->required(),"the device file to read")
            ("reference-file", po::value<std::string>(&reference_file)->default_value(""),"the reference file to read");
    }

    virtual void
    log_extra_help(std::ostream& strm)
    {
        strm << full_sweep_options_;

        strm << random_read_options_;
        strm << "Program exit codes:";
        strm << "0: device and reference file are equal";
        strm << "1: device and reference file are not equal";
        strm << "2: I/O error";
    }

    void
    parse_command_line_arguments()
    {
        parse_unparsed_options(full_sweep_options_,
                               youtils::AllowUnregisteredOptions::T,
                               vm_);

        if(vm_.count("full-sweep"))
        {
            full_sweep = true;
        }

        if(not full_sweep)
        {
            parse_unparsed_options(random_read_options_,
                                   youtils::AllowUnregisteredOptions::T,
                                   vm_);

            if (vm_.count("random-read"))
            {
                random_read = true;
            }
        }
    }

    virtual void
    setup_logging()
    {
        MainHelper::setup_logging();
    }

    virtual int
    run()
    {
        try
        {
            if(full_sweep)
            {
                DeviceReader theReader(device_file,reference_file, read_chance);
                if(theReader())
                {
                    exit(0);
                }
                else
                {
                    exit(1);
                }
            }
            else if (random_read)
            {
                RandomDeviceReader theReader(device_file, reference_file, num_reads);
                if(theReader())
                {
                    exit(0);
                }
                else
                {
                    exit(1);
                }
            }
            else
            {

                // LOG_WARN("Fallback on old command line arguments behaviour, deprecated");

                // if(argc_ != 3)
                // {
                //     std::cerr << "Usage: volumereader <devicefile> <referencefile>\n";
                LOG_FATAL("Option is not one of full_sweep of random_read");

                return 1;
                //     }
                //     DeviceReader theReader(argv_[1],argv_[2]);
                //     if(theReader())
                //     {
                //         exit(0);
                //     }
                //     else
                //     {
                //         exit(1);
                //     }
                // }
            }


        }
        catch(std::exception& e)
        {
            LOG_ERROR( "Caught an exception exiting main: " << e.what());
            std::cerr << e.what() << std::endl;
            return 2;
        }

        catch(...)
        {
            LOG_ERROR( "Caught an unknown exception exiting main");
            return 2;
        }
    }

    double read_chance;
    std::string device_file;
    std::string reference_file;
    uint64_t num_reads;
    bool full_sweep = false;
    bool random_read = false;
    po::options_description full_sweep_options_;
    po::options_description random_read_options_;
};

}

MAIN(VolumeReader)

// Local Variables: **
// compile-command: "make" **
// End: **
