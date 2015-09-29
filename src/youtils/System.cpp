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

#include "Assert.h"
#include "System.h"
#include "pstream.h"

#include <malloc.h>
#include <sys/param.h>
#include <unistd.h>

#include <memory>

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

}

// Local Variables: **
// mode: c++ **
// End: **
