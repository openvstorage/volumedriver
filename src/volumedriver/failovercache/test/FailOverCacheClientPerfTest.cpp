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

#include <youtils/wall_timer.h>
#include <boost/program_options.hpp>
#include <youtils/Logger.h>
#include <fstream>

#include <youtils/BuildInfoString.h>
#include <youtils/Main.h>

#include "volumedriver/Volume.h"
#include "volumedriver/Types.h"

namespace
{
using namespace volumedriver;
using namespace scrubbing;
namespace yt = youtils;
namespace be = backend;
namespace po = boost::program_options;

class FailOverCacheEntryFactory
{
public:
    FailOverCacheEntryFactory(ClusterSize cluster_size,
                              uint32_t num_clusters,
                              ClusterLocation startLocation =  ClusterLocation(1))
        : cluster_size_(cluster_size),
          num_clusters_(num_clusters),
          cluster_loc(startLocation)
    {}

    FailOverCacheEntry*
    operator()(ClusterLocation& next_location,
               const std::string& content = "")
    {
        size_t size = cluster_size_;
        byte* b = new byte[size];
        size_t content_size = content.size();

        if(content_size > 0)
        {
            for(unsigned int i = 0; i < cluster_size_; ++i)
            {
                b[i] = content[i % content_size];
            }
        }

        FailOverCacheEntry* ret = new FailOverCacheEntry(cluster_loc,
                                                         0,
                                                         b,
                                                         cluster_size_);
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



class FailoverCacheClientPerfTestMain : public youtils::MainHelper
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
        MainHelper::setup_logging();
    }

    virtual int
    run()
    {
        LOG_INFO("Run with host " << host_ << " port " << port_ << " namespace " << *ns_ << " sleep micro " << sleep_micro_);
        FailOverCacheBridge failover_bridge(1024, 8);
        Volume * fake_vol = 0;
        failover_bridge.initialize(fake_vol);

        failover_bridge.newCache(std::make_unique<FailOverCacheProxy>(FailOverCacheConfig(host_,
                                                                                          port_),
                                                                      *ns_,
                                                                      4096,
                                                                      FailOverCacheBridge::RequestTimeout));
        failover_bridge.Clear();

        FailOverCacheEntryFactory source(ClusterSize(4096), 1024);
        ClusterLocation next_location;

        uint64_t count = 1000000;
        LOG_INFO("Test entry generation: creating " << count << " entries");
        for(uint64_t i =0; i < count; i++)
        {
            FailOverCacheEntry* e = source(next_location);
            delete e;
            if (i % 100000 == 0)
            {
            }
        }
        LOG_INFO("Finished");

        LOG_INFO("Test FOC throughtput ");

        yt::wall_timer perf_timer;
        double prev_time = perf_timer.elapsed();
        const uint64_t report_interval= 25600;

        uint64_t entry_counter = 0;


        while (true)
        {
            FailOverCacheEntry* e = source(next_location);
            uint64_t cycles = 0;

            while(not failover_bridge.addEntry(e))
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

    std::string host_;
    uint16_t port_;
    std::unique_ptr<backend::Namespace> ns_;
    std::string ns_tmp_;

    uint64_t sleep_micro_;

    po::options_description desc_;
};
}

MAIN(FailoverCacheClientPerfTestMain)

// Local Variables: **
// End: **
