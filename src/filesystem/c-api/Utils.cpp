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

#include "Utils.h"

#include <boost/asio.hpp>

bool
is_volume_name_valid(const char *volume_name)
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

int
hostname_to_ip(const char *hostname, std::string& ip)
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
