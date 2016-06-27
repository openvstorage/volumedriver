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

    ShmIdlInterface::CreateResult*
    create_shm_interface(const ShmIdlInterface::CreateShmArguments& args)
    {
        LOG_INFO("Create shm server for volume '"
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
        LOG_INFO("Stopping shm server for volume '"
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
            LOG_INFO("Stopping shm server for volume '"
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
    create_volume(const char* volume_name,
                  CORBA::ULongLong volume_size)
    {
        std::unique_ptr<Handler> h(new Handler(handler_args_));
        h->create_volume(volume_name,
                         volume_size);
    }

    void
    remove_volume(const char* volume_name)
    {
        std::unique_ptr<Handler> h(new Handler(handler_args_));
        h->remove_volume(volume_name);
    }

    void
    truncate_volume(const char *volume_name,
                    CORBA::ULongLong volume_size)
    {
        std::unique_ptr<Handler> h(new Handler(handler_args_));
        h->truncate_volume(volume_name,
                           volume_size);
    }

    uint64_t
    stat_volume(const char *volume_name)
    {
        std::unique_ptr<Handler> h(new Handler(handler_args_));
        return h->stat_volume(volume_name);
    }

    void
    create_snapshot(const char* volume_name,
                    const char* snapshot_name,
                    CORBA::LongLong timeout)
    {
        std::unique_ptr<Handler> h(new Handler(handler_args_));
        h->create_snapshot(volume_name, snapshot_name, timeout);
    }

    void
    rollback_snapshot(const char* volume_name,
                      const char* snapshot_name)
    {
        std::unique_ptr<Handler> h(new Handler(handler_args_));
        h->rollback_snapshot(volume_name, snapshot_name);
    }

    void
    delete_snapshot(const char* volume_name,
                    const char* snapshot_name)
    {
        std::unique_ptr<Handler> h(new Handler(handler_args_));
        h->delete_snapshot(volume_name, snapshot_name);
    }

    void
    list_snapshots(const char* volume_name,
                   ShmIdlInterface::StringSequence_out results,
                   CORBA::ULongLong_out size)
    {
        std::unique_ptr<Handler> h(new Handler(handler_args_));
        uint64_t c_size;
        std::vector<std::string> snaps(std::move(h->list_snapshots(volume_name,
                                                                   &c_size)));
        results = new ShmIdlInterface::StringSequence(snaps.size());
        results->length(snaps.size());
        for (unsigned int i = 0; i < snaps.size(); i++)
        {
            results[i] = CORBA::string_dup(snaps[i].c_str());
        }
        size = c_size;
    }

    int
    is_snapshot_synced(const char* volume_name,
                       const char* snapshot_name)
    {
        std::unique_ptr<Handler> h(new Handler(handler_args_));
        return int(h->is_snapshot_synced(volume_name, snapshot_name));
    }

    void
    list_volumes(ShmIdlInterface::StringSequence_out results)
    {
        std::unique_ptr<Handler> h(new Handler(handler_args_));
        std::vector<std::string> volumes(std::move(h->list_volumes()));
        results = new ShmIdlInterface::StringSequence(volumes.size());
        results->length(volumes.size());
        for (unsigned int i = 0; i < volumes.size(); i++)
        {
            results[i] = CORBA::string_dup(volumes[i].c_str());
        }
    }

private:
    DECLARE_LOGGER("ShmInterface");

    youtils::OrbHelper& orb_helper_;
    handler_args_t& handler_args_;
    std::mutex shm_servers_lock_;
    std::map<std::string, std::unique_ptr<ServerType>> shm_servers_;
    ShmControlChannelServer shm_ctl_server_;
};

} //namespace volumedriverfs

#endif
