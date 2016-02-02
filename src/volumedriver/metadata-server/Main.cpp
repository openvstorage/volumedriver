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

#include "Manager.h"

#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <youtils/Logging.h>
#include <youtils/Main.h>
#include <youtils/VolumeDriverComponent.h>

#include <backend/BackendConnectionManager.h>

namespace
{

namespace ba = boost::asio;
namespace be = backend;
namespace bs = boost::system;
namespace bpt = boost::property_tree;
namespace po = boost::program_options;
namespace mds = metadata_server;
namespace yt = youtils;

class MetaDataServerMain
    : public yt::MainHelper
{
public:
    MetaDataServerMain(int argc,
                       char** argv)
        : yt::MainHelper(argc,
                         argv)
        , opts_("MetaDataServer options")
    {
        opts_.add_options()
            ("config-file,C",
             po::value<std::string>(&config_file_)->required(),
             "config file (JSON)");
    }

    virtual ~MetaDataServerMain() = default;

    virtual void
    parse_command_line_arguments()
    {
        po::parsed_options
            parsed(parse_unparsed_options(opts_,
                                          yt::AllowUnregisteredOptions::T,
                                          vm_));
    }

    virtual int
    run()
    {
        umask(0);

        ba::io_service ioservice;
        ba::signal_set signals(ioservice, SIGTERM);
        signals.async_wait([&](const bs::error_code& error,
                               int signum)
                           {
                               LOG_INFO("caught signal " << signum << ", error: " << error);
                           });

        const bpt::ptree
            pt(yt::VolumeDriverComponent::read_config_file(config_file_,
                                                           VerifyConfig::T));

        be::BackendConnectionManagerPtr
            cm(be::BackendConnectionManager::create(pt,
                                                    RegisterComponent::T));
        const mds::Manager server(pt,
                                  RegisterComponent::T,
                                  cm);

        LOG_INFO("Server started");

        ioservice.run();

        LOG_INFO("Shutting down the server");

        return 0;
    }

    virtual void
    log_extra_help(std::ostream& os)
    {
        os << opts_ << std::endl;
    }

    virtual void
    setup_logging()
    {
        MainHelper::setup_logging("metadata_server");
    }

private:
    DECLARE_LOGGER("MetaDataServerMain");

    po::options_description opts_;
    std::string config_file_;
};

}

MAIN(MetaDataServerMain)
