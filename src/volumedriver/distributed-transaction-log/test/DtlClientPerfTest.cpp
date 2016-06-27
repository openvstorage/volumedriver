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

#include <youtils/wall_timer.h>
#include <boost/program_options.hpp>
#include <youtils/Logger.h>
#include <fstream>

#include <youtils/BuildInfoString.h>
#include <youtils/Main.h>

#include "volumedriver/FailOverCacheConfig.h"
#include "volumedriver/FailOverCacheClientInterface.h"
#include "volumedriver/Types.h"

namespace
{
using namespace volumedriver;
using namespace scrubbing;
namespace yt = youtils;
namespace be = backend;
namespace po = boost::program_options;

// FailOverCacheEntry does not take ownership of the actual data buffer, so
// cling to it as long as the FailOverCacheEntry is used.
using ClusterHolder = std::pair<FailOverCacheEntry,
                                std::unique_ptr<uint8_t[]>>;

class ClusterFactory
{
public:
    ClusterFactory(ClusterSize cluster_size,
                   uint32_t num_clusters,
                   ClusterLocation startLocation = ClusterLocation(1))
        : cluster_size_(cluster_size)
        , num_clusters_(num_clusters)
        , cluster_loc(startLocation)
    {}

    ClusterHolder
    operator()(ClusterLocation& next_location,
               const std::string& content = "")
    {
        size_t size = cluster_size_;
        auto buf(std::make_unique<uint8_t[]>(size));
        size_t content_size = content.size();

        if(content_size > 0)
        {
            for(unsigned int i = 0; i < cluster_size_; ++i)
            {
                buf[i] = content[i % content_size];
            }
        }

        FailOverCacheEntry entry(cluster_loc,
                                 0,
                                 buf.get(),
                                 cluster_size_);

        ClusterHolder
            ret(std::make_pair(entry,
                               std::move(buf)));

        SCOOffset a = cluster_loc.offset();

        if(a  >= num_clusters_ -1)
        {
            cluster_loc.incrementNumber();
            cluster_loc.offset(0);
        }
        else
        {
            cluster_loc.incrementOffset();
        }

        next_location = cluster_loc;
        return ret;
    }

private:
    const ClusterSize cluster_size_;
    const uint32_t num_clusters_;
    ClusterLocation cluster_loc;
};

class FailoverCacheClientPerfTestMain
    : public youtils::MainHelper
{
public:
    FailoverCacheClientPerfTestMain(int argc,
                                    char** argv)
        : MainHelper(argc,
                     argv)
        , desc_("Required Options")
    {
        desc_.add_options()
                ("host",
                 po::value<std::string>(&host_)->required(),
                 "host of the failovercache server")
                ("port",
                 po::value<uint16_t>(&port_)->required(),
                 "port of the failovercache server")
                ("mode",
                 po::value<volumedriver::FailOverCacheMode>(&mode_)->default_value(FailOverCacheMode::Asynchronous),
                 "mode of the failovercache server (Asynchronous|Synchronous)")
                ("namespace",
                 po::value<std::string>(&ns_tmp_)->required(),
                 "namespace to use for testing")
                 ("sleep_micro",
                  po::value<uint64_t>(&sleep_micro_)->default_value(1000),
                  "amount to sleep in microseconds when failovercache can't follow");
    }

    virtual void
    log_extra_help(std::ostream& strm)
    {
        strm << desc_;
    }

    virtual void
    parse_command_line_arguments()
    {
        parse_unparsed_options(desc_,
                               youtils::AllowUnregisteredOptions::T,
                               vm_);
        ns_.reset(new backend::Namespace(ns_tmp_));

    }

    void
    setup_logging()
    {
        MainHelper::setup_logging("dtl_client_perf_test");
    }

    bool
    addEntry(FailOverCacheClientInterface& foc,
             const FailOverCacheEntry& e)
    {
        std::vector<ClusterLocation> locs;
        locs.emplace_back(e.cli_);
        return foc.addEntries(locs,
                              1,
                              e.lba_,
                              e.buffer_);
    }

    virtual int
    run()
    {
        LOG_INFO("Run with host " << host_ << " port " << port_ << " mode " << mode_ << " namespace " << *ns_ << " sleep micro " << sleep_micro_);
        max_entries_ = 1024;
        write_trigger_ = 8;
        const LBASize lba_size(512);
        const ClusterMultiplier cmult(8);

        std::unique_ptr<FailOverCacheClientInterface>
            failover_bridge(FailOverCacheClientInterface::create(mode_,
                                                                 lba_size,
                                                                 cmult,
                                                                 max_entries_,
                                                                 write_trigger_));

        failover_bridge->initialize([&]() noexcept
                                    {
                                        LOG_WARN("Got a DEGRADED event");
                                    });

        failover_bridge->newCache(std::make_unique<DtlProxy>(FailOverCacheConfig(host_,
                                                                                           port_,
                                                                                           mode_),
                                                                       *ns_,
                                                                       lba_size,
                                                                       cmult,
                                                                       failover_bridge->getDefaultRequestTimeout()));
        failover_bridge->Clear();

        ClusterFactory source(ClusterSize(lba_size * cmult),
                              1024);
        ClusterLocation next_location;

        uint64_t count = 1000000;
        LOG_INFO("Test entry generation: creating " << count << " entries");
        for(uint64_t i =0; i < count; i++)
        {
            const ClusterHolder e = source(next_location);
            if (i % 100000 == 0)
            {
            }
        }
        LOG_INFO("Finished");

        LOG_INFO("Test FOC throughtput");

        yt::wall_timer perf_timer;
        double prev_time = perf_timer.elapsed();
        const uint64_t report_interval= 25600;

        uint64_t entry_counter = 0;

        while (true)
        {
            const ClusterHolder ch = source(next_location);
            uint64_t cycles = 0;

            while(not addEntry(*failover_bridge,
                               ch.first))
            {
                if (cycles++ == 0)
                {
                    LOG_DEBUG("starting to throttle");
                }
                usleep(sleep_micro_);
            }
            if (cycles > 0)
            {
                LOG_DEBUG("finished throttling, took " << cycles << " cycles");
            }

            if (entry_counter > 0 and (entry_counter % report_interval == 0))
            {
                double tmp = perf_timer.elapsed();
                LOG_INFO("throughput " << report_interval * 4096 / (tmp - prev_time) / 1024/ 1024 << "MB/s");
                prev_time = tmp;
            }
            entry_counter++;
        }

        return 0;
    }

    std::atomic<unsigned> max_entries_;
    std::atomic<unsigned> write_trigger_;
    std::string host_;
    uint16_t port_;
    volumedriver::FailOverCacheMode mode_;
    std::unique_ptr<backend::Namespace> ns_;
    std::string ns_tmp_;

    uint64_t sleep_micro_;

    po::options_description desc_;
};
}

MAIN(FailoverCacheClientPerfTestMain)

// Local Variables: **
// End: **
