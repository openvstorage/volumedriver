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

#include "../ScrubberAdapter.h"
#include "../ScrubReply.h"
#include "../ScrubWork.h"

#include <sys/prctl.h>

#include <youtils/Logging.h>
#include <youtils/Main.h>
#include <youtils/SignalBlocker.h>
#include <youtils/SignalSet.h>
#include <youtils/SignalThread.h>

namespace
{

namespace be = backend;
namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace sc = scrubbing;
namespace yt = youtils;

class Main
    : public yt::MainHelper
{
    DECLARE_LOGGER("ScrubberMain");

    po::options_description opts_;
    std::string scrub_work_str_;
    fs::path scratch_dir_;
    uint64_t region_size_exponent_ =
        scrubbing::ScrubberAdapter::region_size_exponent_default;
    float fill_ratio_ = scrubbing::ScrubberAdapter::fill_ratio_default;
    bool verbose_ = scrubbing::ScrubberAdapter::verbose_scrubbing_default;

public:
    Main(int argc,
         char** argv)
        : yt::MainHelper(argc,
                         argv)
        , opts_("Scrubber options")
    {
        opts_.add_options()
            ("scrub-work",
             po::value<std::string>(&scrub_work_str_)->required(),
             "scrub work description (serialized to a string, obtained from volumedriver")
            ("scratch-dir",
             po::value<fs::path>(&scratch_dir_)->required(),
             "directory for temporary files")
            ("region-size-exponent",
             po::value<uint64_t>(&region_size_exponent_)->default_value(region_size_exponent_),
             "number of clusters in a region as power of two")
            ("fill-ratio",
             po::value<float>(&fill_ratio_)->default_value(fill_ratio_),
             "SCOs with larger fill ratios will not be scrubbed")
            ("verbose",
             po::value<bool>(&verbose_)->default_value(verbose_),
             "verbose logging during scrubbing")
             ;
    }

    virtual void
    log_extra_help(std::ostream& os) override final
    {
        os << opts_;
    }

    virtual void
    parse_command_line_arguments() override final
    {
        parse_unparsed_options(opts_,
                               yt::AllowUnregisteredOptions::T,
                               vm_);
    }

    virtual void
    setup_logging() override final
    {
        yt::MainHelper::setup_logging("scrubber");
    }

    virtual int
    run() override final
    {
        using namespace scrubbing;

        const int signal = SIGUSR2;

        const yt::SignalSet sigset({signal});
        const yt::SignalBlocker sigblocker(sigset);
        const yt::SignalThread sigthread(sigset,
                                         [&](int sig)
                                         {
                                             LOG_FATAL("Received signal " << sig <<
                                                       " - terminating");
                                             std::exit(EXIT_FAILURE);
                                         });

        int ret = ::prctl(PR_SET_PDEATHSIG,
                          signal);
        if (ret != 0)
        {
            ret = errno;
            LOG_FATAL("Could not set parent death signal: " << strerror(ret));
            return ret;
        }

        std::unique_ptr<be::BackendConfig> bcfg;
        if (yt::MainHelper::backend_config_)
        {
            bcfg = be::BackendConfig::makeBackendConfig(*yt::MainHelper::backend_config_);
        }

        const ScrubReply reply(ScrubberAdapter::scrub(std::move(bcfg),
                                                      ScrubWork(scrub_work_str_),
                                                      scratch_dir_,
                                                      region_size_exponent_,
                                                      fill_ratio_,
                                                      false,
                                                      verbose_));

        std::cout << reply.str() << std::endl;

        return 0;
    }
};

}

MAIN(Main);
