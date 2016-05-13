// Copyright 2016 iNuron NV
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

#ifndef __NETWORK_XIO_INTERFACE_H_
#define __NETWORK_XIO_INTERFACE_H_

#include <boost/property_tree/ptree_fwd.hpp>
#include <unordered_map>
#include <libxio.h>

#include <youtils/VolumeDriverComponent.h>

#include "NetworkXioServer.h"

namespace volumedriverfs
{

class NetworkXioInterface
    : public youtils::VolumeDriverComponent
{
public:
    NetworkXioInterface(const boost::property_tree::ptree& pt,
                        const RegisterComponent registerizle,
                        FileSystem& fs)
    : youtils::VolumeDriverComponent(registerizle,
                                     pt)
    , network_uri(pt)
    , network_snd_rcv_queue_depth(pt)
    , fs_(fs)
    , xio_server_(fs_,
                  uri(),
                  snd_rcv_queue_depth())
    {}

    ~NetworkXioInterface()
    {
        shutdown();
    }

    NetworkXioInterface(const NetworkXioInterface&) = delete;

    NetworkXioInterface&
    operator=(const NetworkXioInterface&) = delete;

    virtual const char*
    componentName() const override final;

    virtual void
    update(const boost::property_tree::ptree& pt,
           youtils::UpdateReport& rep) override final;

    virtual void
    persist(boost::property_tree::ptree& pt,
            const ReportDefault reportDefault) const override final;

    virtual bool
    checkConfig(const boost::property_tree::ptree&,
                youtils::ConfigurationReport&) const override final;

    void
    run();

    void
    shutdown();

    std::string
    uri() const
    {
        return network_uri.value();
    }

    size_t
    snd_rcv_queue_depth() const
    {
        return network_snd_rcv_queue_depth.value();
    }
private:
    DECLARE_LOGGER("NetworkXioInterface");

    DECLARE_PARAMETER(network_uri);
    DECLARE_PARAMETER(network_snd_rcv_queue_depth);

    FileSystem& fs_;
    NetworkXioServer xio_server_;
};

} //namespace volumedriverfs

#endif //__NETWORK_XIO_INTERFACE_H_
