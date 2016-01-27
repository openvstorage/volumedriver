// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "Assert.h"
#include "System.h"

#include <malloc.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include <memory>

#include <pstreams/pstream.h>

namespace youtils
{

std::string
System::getHostName()
{
    std::unique_ptr<char[]> buf(new char[MAXHOSTNAMELEN]);
    int res = gethostname(buf.get(), MAXHOSTNAMELEN);
    VERIFY(res == 0);
    std::string str(buf.get());
    return str;
}

uint64_t
System::getPID()
{
    return getpid();
}

void
System::malloc_info(const boost::filesystem::path& outfile)
{
    int err = 0;
    FILE* fp = ::fopen(outfile.string().c_str(), "w");
    if (fp == nullptr)
    {
        err = errno;
        LOG_ERROR("Failed to open " << outfile << ": " << strerror(err));
        throw fungi::IOException("failed to open file for malloc_info",
                                 outfile.string().c_str(),
                                 err);
    }

    BOOST_SCOPE_EXIT((&fp))
    {
        ::fclose(fp);
    }
    BOOST_SCOPE_EXIT_END;

    err = ::malloc_info(0, fp);
    if (err)
    {
        err = errno;
        LOG_ERROR("malloc_info failed: " << strerror(err));
        throw fungi::IOException("malloc_info failed");
    }
}

std::string
System::malloc_info()
{
    int err = 0;
    char* data = nullptr;
    size_t size = 0;

    BOOST_SCOPE_EXIT((&data))
    {
        ::free(data);
    }
    BOOST_SCOPE_EXIT_END;

    {
        FILE* fp = ::open_memstream(&data, &size);
        if (fp == nullptr)
        {
            err = errno;
            LOG_ERROR("Failed to open memstream: " << strerror(err));
            throw fungi::IOException("Failed to open memstream");
        }

        BOOST_SCOPE_EXIT((&fp))
        {
            ::fclose(fp);
        }
        BOOST_SCOPE_EXIT_END;

        err = ::malloc_info(0, fp);
        if (err)
        {
            err = errno;
            LOG_ERROR("malloc_info failed: " << strerror(err));
            throw fungi::IOException("malloc_info failed");
        }
    }

    LOG_TRACE("size: " << size << ", data: '" << data << "'");
    return std::string(data, size);
}

bool
System::system_command(const std::string& command)
{
    errno = 0;
    int i = ::system(command.c_str());
    if(i != 0)
    {
        i = errno;
        LOG_ERROR("Could not execute system call: '"  << command << "'errno: " <<
                  i << ". errstr: " << strerror(i));
        return false;
    }
    else
    {
        LOG_TRACE("System call succeeded: '" << command << "'");
        return true;
    }
}

std::pair<std::string, int>
System::exec(const std::string& command)
{
    int rc;
    std::string buffer;
    redi::ipstream cmd(command);

    while (cmd >> buffer);

    if (cmd.rdbuf()->exited())
    {
        rc = cmd.rdbuf()->status();
    }

    return std::make_pair(buffer, rc);
}

void
System::setup_tcp_keepalive(int fd,
                            int keep_cnt,
                            int keep_idle,
                            int keep_intvl)
{
    VERIFY(fd >= 0);

    auto set([&](const char* desc,
                 int level,
                 int optname,
                 int val)
             {
                 int ret = ::setsockopt(fd,
                                        level,
                                        optname,
                                        &val,
                                        sizeof(val));
                 if (ret < 0)
                 {
                     ret = errno;
                     LOG_ERROR("Failed to set " << desc << " on socket " <<
                               fd << ": " << strerror(errno));
                     throw fungi::IOException("Failed to set socket option",
                                              desc);
                 }
             });

        set("KeepAlive",
            SOL_SOCKET,
            SO_KEEPALIVE,
            1);

        set("keepalive probes",
            SOL_TCP,
            TCP_KEEPCNT,
            keep_cnt);

        set("keepalive time",
            SOL_TCP,
            TCP_KEEPIDLE,
            keep_idle);

        set("keepalive interval",
            SOL_TCP,
            TCP_KEEPINTVL,
            keep_intvl);
}

}

// Local Variables: **
// mode: c++ **
// End: **
