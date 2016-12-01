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

#ifndef __NETWORK_XIO_INTERFACE_H_
#define __NETWORK_XIO_INTERFACE_H_

#include "NetworkXioServer.h"

#include <boost/property_tree/ptree_fwd.hpp>
#include <youtils/VolumeDriverComponent.h>

#include <libxio.h>
#include <unordered_map>

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
    , network_workqueue_max_threads(pt)
    , network_workqueue_ctrl_max_threads(pt)
    , network_max_neighbour_distance(pt)
    , fs_(fs)
    , xio_server_(fs_,
                  uri(),
                  snd_rcv_queue_depth(),
                  wq_max_threads(),
                  wq_ctrl_max_threads(),
                  max_neighbour_distance())
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
    run(std::promise<void> promise);

    void
    shutdown();

    youtils::Uri
    uri() const;

    size_t
    snd_rcv_queue_depth() const
    {
        return network_snd_rcv_queue_depth.value();
    }

    unsigned int
    wq_max_threads() const
    {
        return network_workqueue_max_threads.value();
    }

    unsigned int
    wq_ctrl_max_threads() const
    {
        return network_workqueue_ctrl_max_threads.value();
    }

    const std::atomic<uint32_t>&
    max_neighbour_distance() const
    {
        return network_max_neighbour_distance.value();
    }

private:
    DECLARE_LOGGER("NetworkXioInterface");

    DECLARE_PARAMETER(network_uri);
    DECLARE_PARAMETER(network_snd_rcv_queue_depth);
    DECLARE_PARAMETER(network_workqueue_max_threads);
    DECLARE_PARAMETER(network_workqueue_ctrl_max_threads);
    DECLARE_PARAMETER(network_max_neighbour_distance);

    FileSystem& fs_;
    NetworkXioServer xio_server_;
};

} //namespace volumedriverfs

#endif //__NETWORK_XIO_INTERFACE_H_
