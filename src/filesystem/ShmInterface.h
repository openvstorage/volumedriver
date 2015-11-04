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

#include <youtils/OrbHelper.h>
#include "ShmIdlInterface.h"
#include "ShmServer.h"

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
    {}

    ~ShmInterface()
    {}

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
            TODO("cnanakos: we should have cleaned up here if we faced a client error");
            LOG_ERROR("Creating shm server failed, volume '" << args.volume_name
                      << "' already present");
            throw ShmIdlInterface::VolumeNameAlreadyRegistered(volume_name.c_str());
        }

        std::unique_ptr<Handler> h;

        try
        {
            h.reset(new Handler(args, handler_args_));
        }
        catch (ShmIdlInterface::VolumeDoesNotExist& e)
        {
            LOG_ERROR("Volume '" << volume_name << "'does not exist");
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

        size_t erased = shm_servers_.erase(volume_name);
        if (erased == 0)
        {
            throw ShmIdlInterface::NoSuchVolumeRegistered(volume_name);
        }
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
    std::map<std::string, std::unique_ptr<ServerType>> shm_servers_;
};

}

#endif
