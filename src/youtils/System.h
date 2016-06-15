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

#ifndef YOUTILS_SYSTEM_H_
#define YOUTILS_SYSTEM_H_

#include "Assert.h"
#include "Logging.h"

#include <string>
#include <cstdlib>
#include <cstdio>
#include <future>

#include <unistd.h>

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/scope_exit.hpp>
#include <boost/optional.hpp>
namespace youtils
{

struct System
{
    DECLARE_LOGGER("System");

    static std::string
    getHostName();

    static uint64_t
    getPID();

    static bool
    get_bool_from_env(const std::string& name,
                      const bool default_value)
    {
        const char* val = getenv(name.c_str());

        if(val == 0)
        {
            return default_value;
        }
        else
        {
            // REGEX MATCHING WILL BE FASTER but probably not worth it
            std::string read(val);
            if(read == "yes" or
               read == "YES" or
               read == "Y" or
               read == "T" or
               read == "TRUE" or
               read == "true")
            {
                return true;
            }
            else if(read == "no" or
                    read == "NO" or
                    read == "N" or
                    read == "F" or
                    read == "FALSE" or
                    read == "false")
            {
                return false;
            }
            else
            {
                LOG_ERROR("Environment variable " << name
                          << " has value " << read
                          << " which is not recognized\n"
                          << "try on of yes,YES,Y,T,TRUE,true,no,NO,N,F,FALSE or false");


                throw fungi::IOException("Unrecognized environmet variable value, cannot interpret it als boolean");
            }
        }

    }

    // don't use with (u)char or (u)int8_t - it'll come back and haunt you.
    // also not so good : pass optional as default -> requires copy initialization of T.
    template<typename T>
    static T
    get_env_with_default(const std::string& name,
                         const T& default_value)
    {
        typedef typename std::remove_cv<T>::type T_;

        static_assert(not (std::is_same<T_, uint8_t>::value or
                           std::is_same<T_, int8_t>::value or
                           std::is_same<T_, char>::value or
                           std::is_same<T_, unsigned char>::value),
                      "this doesn't work with byte sized types");

        const char* val = getenv(name.c_str());

        if(val == 0)
        {
            return default_value;

            // if(not default_value)
            // {
            //     throw std::exception("No such env var and no default value");
            // }
            // else
            // {

            //     return default_value;
            // }

        }
        else
        {
            try
            {
                return static_cast<T>(boost::lexical_cast<T>(val));
            }
            catch(...)
            {
                return default_value;
            }
        }
    }

    template<typename T>
    static void
    set_env(const std::string& name,
            const T& value,
            const bool replace)
    {
        VERIFY(not name.empty());
        std::string out = boost::lexical_cast<std::string, T>(value);
        int res = setenv(name.c_str(),
                         out.c_str(),
                         replace);
        VERIFY(not res);

    }

    static void
    unset_env(const std::string& name)
    {
        VERIFY(not name.empty());
        int res = unsetenv(name.c_str());
        VERIFY(not res);
    }

    template<typename F>
    static std::string
    with_captured_c_fstream(FILE* fp, F&& fun)
    {
        int ret = ::fflush(fp);
        if (ret != 0)
        {
            ret = errno;
            LOG_ERROR("Failed to flush stream: " << strerror(ret));
            throw fungi::IOException("Failed to flush stream",
                                     strerror(ret));
        }

        int fd = fileno(fp);
        if (fd < 0)
        {
            LOG_ERROR("Failed to get file descriptor from file pointer: " <<
                      strerror(errno));
            throw fungi::IOException("Failed to get file descriptor from file pointer");
        }

        const int old_fd = ::dup(fd);
        if (old_fd < 0)
        {
            throw fungi::IOException("Failed to dup file descriptor",
                                     strerror(errno));
        }

        BOOST_SCOPE_EXIT_TPL((old_fd)(fd))
        {
            ::dup2(old_fd, fd);
        }
        BOOST_SCOPE_EXIT_END;

        int out_pipe[2] = { -1, -1 };
        ret = ::pipe(out_pipe);
        if (ret < 0)
        {
            throw fungi::IOException("Failed to create capture pipe",
                                     strerror(errno));
        }

        BOOST_SCOPE_EXIT_TPL((&out_pipe))
        {
            ::close(out_pipe[0]);
            out_pipe[0] = -1;
            ::close(out_pipe[1]);
            out_pipe[1] = -1;
        }
        BOOST_SCOPE_EXIT_END;

        ret = ::dup2(out_pipe[1], fd);
        if (ret < 0)
        {
            throw fungi::IOException("Failed to dup2 write end of pipe to fd",
                                     strerror(errno));
        }

        ret = ::close(out_pipe[1]);
        out_pipe[1] = -1;
        if (ret < 0)
        {
            throw fungi::IOException("Failed to close old write end of pipe",
                                     strerror(errno));
        }

        std::stringstream ss;

        // read asynchronously from the pipe while the writer fills it to prevent
        // deadlock once the pipe buffer is filled up
        auto future(std::async(std::launch::async,
                               [&]()
                               {
                                   std::vector<char> rbuf(4096);
                                   ssize_t r;
                                   do
                                   {
                                       r = ::read(out_pipe[0], rbuf.data(), rbuf.size());
                                       if (r < 0)
                                       {
                                           throw fungi::IOException("Failed to read from pipe");
                                       }
                                       else if (r > 0)
                                       {
                                           ss.write(rbuf.data(), r);
                                       }
                                   }
                                   while (r > 0);
                               }));

        fun();

        ret = ::fflush(fp);
        int flush_err = ret ? errno : 0;
        ::close(fd); // make sure we break out of the read loop

        if (ret != 0)
        {
            throw fungi::IOException("Failed to flush file pointer", strerror(flush_err));
        }

        future.get(); // rethrow exceptions if any
        return ss.str();
    }

    static void
    malloc_info(const boost::filesystem::path& outfile);

    static std::string
    malloc_info();

    static bool
    system_command(const std::string& command);

    static std::pair<std::string, int>
    exec(const std::string& command);

    static void
    setup_tcp_keepalive(int fd,
                        int keep_cnt,
                        int keep_idle,
                        int keep_intvl);
};

}

#endif // !YOUTILS_SYSTEM_H_

// Local Variables: **
// mode: c++ **
// End: **
