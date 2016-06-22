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

#ifndef __CONTEXT_H
#define __CONTEXT_H

#include <boost/asio.hpp>

#include "NetworkXioClient.h"

#include "../ShmControlChannelProtocol.h"

struct ovs_context_t
{
    TransportType transport;
    int oflag;

    virtual ~ovs_context_t() {};

    virtual int open_volume(const char *volume_name,
                            int oflag) = 0;

    virtual void close_volume() = 0;

    virtual int create_volume(const char *volume_name,
                              uint64_t size) = 0;

    virtual int remove_volume(const char *volume_name) = 0;

    virtual int snapshot_create(const char *volume_name,
                                const char *snapshot_name,
                                const uint64_t timeout) = 0;

    virtual int snapshot_rollback(const char *volume_name,
                                  const char *snapshot_name) = 0;

    virtual int snapshot_remove(const char *volume_name,
                                const char *snapshot_name) = 0;

    virtual void list_snapshots(std::vector<std::string>& snaps,
                                const char *volume_name,
                                uint64_t *size,
                                int *saved_errno) = 0;

    virtual int is_snapshot_synced(const char *volume_name,
                                   const char *snapshot_name) = 0;

    virtual int list_volumes(std::vector<std::string>& volumes) = 0;

    virtual int send_read_request(struct ovs_aiocb *ovs_aiocbp,
                                  ovs_aio_request *request) = 0;

    virtual int send_write_request(struct ovs_aiocb *ovs_aiocbp,
                                   ovs_aio_request *request) = 0;

    virtual int send_flush_request(ovs_aio_request *request) = 0;

    virtual int stat_volume(struct stat *st) = 0;

    virtual ovs_buffer_t* allocate(size_t size) = 0;

    virtual int deallocate(ovs_buffer_t *ptr) = 0;

    virtual int truncate_volume(const char *volume_name,
                                uint64_t length) = 0;

    virtual int truncate(uint64_t length) = 0;
};

static bool
_is_volume_name_valid(const char *volume_name)
{
    if (volume_name == NULL || strlen(volume_name) == 0 ||
        strlen(volume_name) >= NAME_MAX)
    {
        return false;
    }
    else
    {
        return true;
    }
}

static inline int
_hostname_to_ip(const char *hostname, std::string& ip)
{

    try
    {
        boost::asio::io_service io_service;
        boost::asio::ip::tcp::resolver resolver(io_service);
        boost::asio::ip::tcp::resolver::query query(std::string(hostname), "");
        for (boost::asio::ip::tcp::resolver::iterator i = resolver.resolve(query);
                i != boost::asio::ip::tcp::resolver::iterator(); i++)
        {
            boost::asio::ip::tcp::endpoint ep = *i;
            ip = ep.address().to_string();
            return 0;
        }
        errno = EINVAL;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
    }
    catch (...)
    {
        errno = EIO;
    }
    return -1;
}
#endif // __CONTEXT_H
