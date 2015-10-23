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

struct ClusterHolder // This is the old FailOverCacheEntry... (as it must hold the memory of the cluster)
{
    ClusterHolder(ClusterLocation cli,
                  uint64_t lba,
                  const uint8_t* buffer,
                  uint32_t size)
        : cli_(cli)
        , lba_(lba)
        , size_(size)
        , buffer_(new byte[size])
    {
        memcpy(&buffer_[0], buffer, size_);
    }

    ClusterHolder(ClusterLocation cli,
                  uint64_t lba,
                  byte* buffer,
                  uint32_t size)
        : cli_(cli)
        , lba_(lba)
        , size_(size)
        , buffer_(buffer)
    {
    }

    ClusterHolder()
        : lba_(0)
        , size_(0)
        , buffer_(0)
    {
    }

    ClusterHolder(const ClusterHolder& other)
        : cli_(other.cli_)
        , lba_(other.lba_)
        , size_(other.size_)
        , buffer_(new byte[size_])
    {
        memcpy(&buffer_[0], &other.buffer_[0], size_);
    }

    ClusterHolder(ClusterHolder&& other)
        : cli_(std::move(other.cli_))
        , lba_(other.lba_)
        , size_(other.size_)
        , buffer_(0)
    {
        other.size_ = 0;
        buffer_.swap(other.buffer_);
    }

    ClusterHolder& operator=(ClusterHolder&& other)
    {
        if (this != &other)
        {
            std::swap(cli_, other.cli_);
            std::swap(lba_, other.lba_);
            std::swap(size_, other.size_);
            buffer_.swap(other.buffer_);
        }
        return *this;
    }


    //ClusterHolder& operator=(const ClusterHolder&)
    //{
    //
    //}

    ClusterLocation cli_;
    uint64_t lba_;
    uint32_t size_;
    boost::scoped_array<byte> buffer_;
};

class ClusterFactory
{
public:
    ClusterFactory(ClusterSize cluster_size,
                              uint32_t num_clusters,
                              ClusterLocation startLocation =  ClusterLocation(1))
        : cluster_size_(cluster_size),
          num_clusters_(num_clusters),
          cluster_loc(startLocation)
    {}

    ClusterHolder
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

        ClusterHolder ret(cluster_loc,
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

    bool addEntry(FailOverCacheClientInterface& focItf, ClusterHolder& ch)
    {
        std::vector<ClusterLocation> locs;
        locs.emplace_back(ch.cli_);
        return focItf.addEntries(locs, 1, ch.lba_, ch.buffer_.get());
    }

    virtual int
    run()
    {
        LOG_INFO("Run with host " << host_ << " port " << port_ << " namespace " << *ns_ << " sleep micro " << sleep_micro_);
        FailOverCacheClientInterface& failover_bridge = *FailOverCacheBridgeFactory::create(FailOverCacheMode::Asynchronous, 1024, 8);
        Volume * fake_vol = 0;
        failover_bridge.initialize(fake_vol);

        failover_bridge.newCache(std::make_unique<FailOverCacheProxy>(FailOverCacheConfig(host_,
                                                                                          port_),
                                                                      *ns_,
                                                                      4096,
                                                                      failover_bridge.getDefaultRequestTimeout()));
        failover_bridge.Clear();

        ClusterFactory source(ClusterSize(4096), 1024);
        ClusterLocation next_location;

        uint64_t count = 1000000;
        LOG_INFO("Test entry generation: creating " << count << " entries");
        for(uint64_t i =0; i < count; i++)
        {
            ClusterHolder e = source(next_location);
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
            ClusterHolder ch = source(next_location);
            uint64_t cycles = 0;

            while(not addEntry(failover_bridge, ch))
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
