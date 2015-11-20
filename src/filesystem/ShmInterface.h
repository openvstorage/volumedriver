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

#ifndef __SHM_INTERFACE_H
#define __SHM_INTERFACE_H

#include <mutex>
#include <youtils/OrbHelper.h>

#include "ShmIdlInterface.h"
#include "ShmServer.h"
#include "ShmControlChannelServer.h"

namespace volumedriverfs
{

template<typename Handler>
class ShmInterface
{
    typedef typename Handler::construction_args handler_args_t;

public:
    ShmInterface(youtils::OrbHelper& orb_helper,
                 handler_args_t& handler_args)
        : orb_helper_(orb_helper)
        , handler_args_(handler_args)
        , shm_ctl_server_(ShmConnectionDetails::Endpoint())
    {
        shm_ctl_server_.create_control_channel(boost::bind(&ShmInterface::try_stop_volume,
                                                           this,
                                                           _1),
                                               boost::bind(&ShmInterface::is_volume_valid,
                                                           this,
                                                           _1,
                                                           _2));
    }

    ~ShmInterface()
    {
        shm_ctl_server_.destroy_control_channel();
    }

    typedef ShmServer<Handler> ServerType;

    DECLARE_LOGGER("ShmInterface");

    ShmIdlInterface::CreateResult*
    create_shm_interface(const ShmIdlInterface::CreateShmArguments& args)
    {
        LOG_INFO("Create shm server for '"
                 << args.volume_name << "'");

        const std::string volume_name(args.volume_name);
        ShmIdlInterface::CreateResult_var create_result(new ShmIdlInterface::CreateResult());

        if (shm_servers_.count(volume_name) != 0)
        {
            LOG_ERROR("Creating shm server failed, volume '" << args.volume_name
                      << "' already present and open");
            throw ShmIdlInterface::VolumeNameAlreadyRegistered(volume_name.c_str());
        }

        std::unique_ptr<Handler> h;

        try
        {
            h.reset(new Handler(args, handler_args_));
        }
        catch (ShmIdlInterface::VolumeDoesNotExist& e)
        {
            LOG_ERROR("Volume '" << volume_name << "' does not exist");
            throw;
        }
        catch (std::exception& e)
        {
            LOG_ERROR("Creating handler failed: " << e.what());
            throw ShmIdlInterface::CouldNotCreateHandler(volume_name.c_str(),
                                                         e.what());
        }
        VERIFY(h);

        create_result->volume_size_in_bytes = h->volume_size_in_bytes();

        std::lock_guard<std::mutex> lock_(shm_servers_lock_);
        shm_servers_.emplace(volume_name,
                             std::unique_ptr<ServerType>(new ServerType(std::move(h))));

        create_result->writerequest_uuid = shm_servers_[volume_name]->writerequest_uuid().str().c_str();
        create_result->writereply_uuid = shm_servers_[volume_name]->writereply_uuid().str().c_str();
        create_result->readrequest_uuid = shm_servers_[volume_name]->readrequest_uuid().str().c_str();
        create_result->readreply_uuid = shm_servers_[volume_name]->readreply_uuid().str().c_str();
        return create_result._retn();
    }

    void
    stop_volume(const char* volume_name)
    {
        LOG_INFO("Stopping shm server for '"
                 << volume_name << "'");

        std::lock_guard<std::mutex> lock_(shm_servers_lock_);
        size_t erased = shm_servers_.erase(volume_name);
        if (erased == 0)
        {
            throw ShmIdlInterface::NoSuchVolumeRegistered(volume_name);
        }
    }

    bool
    try_stop_volume(const std::string& volume_name)
    {
        std::lock_guard<std::mutex> lock_(shm_servers_lock_);
        auto it = shm_servers_.find(volume_name);
        if (it != shm_servers_.end())
        {
            LOG_INFO("Stopping shm server for '"
                     << volume_name << "'");
            shm_servers_.erase(it);
            return true;
        }
        return false;
    }

    bool
    is_volume_valid(const std::string& volume_name,
                    const std::string& key)
    {
        std::lock_guard<std::mutex> lock_(shm_servers_lock_);
        auto it = shm_servers_.find(volume_name);
        if (it != shm_servers_.end())
        {
            if (it->second->writerequest_uuid().str() == key)
            {
                return true;
            }
        }
        return false;
    }

    void
    stop_all_and_exit()
    {
        LOG_INFO("Stop all shm servers");
        shm_servers_.clear();
        orb_helper_.stop();
    }

    void
    create_volume(const char *volume_name,
                  CORBA::ULongLong volume_size)
    {
        std::unique_ptr<Handler> h(new Handler(handler_args_));
        h->create_volume(volume_name,
                         volume_size);
    }

private:
    youtils::OrbHelper& orb_helper_;
    handler_args_t& handler_args_;
    std::mutex shm_servers_lock_;
    std::map<std::string, std::unique_ptr<ServerType>> shm_servers_;
    ShmControlChannelServer shm_ctl_server_;
};

} //namespace volumedriverfs

#endif
