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

#ifndef YT_URL_H_
#define YT_URL_H_

#include "Assert.h"

#include <iosfwd>
#include <string>
#include <iostream>

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

namespace youtils
{

using namespace std::literals::string_literals;

template<typename T>
struct Url
{
    static const uint16_t default_port;
    static const std::string proto;

    std::string host;
    uint16_t port = default_port;
    std::string key;


    explicit Url(std::string h,
                 uint16_t p = default_port,
                 std::string k = std::string("/"))
        : host(std::move(h))
        , port(p)
        , key(std::move(k))
    {}

    Url() = default;

    ~Url() = default;

    Url(const Url&) = default;

    Url&
    operator=(const Url&) = default;

    Url(Url&&) = default;

    Url&
    operator=(Url&&) = default;

    bool
    operator==(const Url& other) const
    {
        return
            host == other.host and
            port == other.port and
            key == other.key;
    }

    bool
    operator!=(const Url& other) const
    {
        return not operator==(other);
    }

    friend std::ostream&
    operator<<(std::ostream& os,
               const Url& url)
    {
        os << proto <<
            "://" << url.host <<
            ":" << url.port;
        if (not url.key.empty())
        {
            os << url.key;
        }
        return os;
    }

    friend std::istream&
    operator>>(std::istream& is,
               Url& url)
    {
        std::string str;
        is >> str;
        boost::regex rex("("s + proto + ")://([^/ :]+):?([^/ ]*)(/?[^ #?]*)"s);

        boost::smatch match;
        if (boost::regex_match(str,
                               match,
                               rex))
        {
            ASSERT(proto == match[1]);

            url.host = match[2];

            if (not match[3].str().empty())
            {
                url.port = boost::lexical_cast<uint16_t>(match[3]);
            }

            if (not match[4].str().empty())
            {
                url.key = match[4];
            }
            else
            {
                url.key = "/"s;
            }
        }
        else
        {
            is.setstate(std::ios::failbit);
        }
        return is;
    }

    static bool
    is_one(const std::string& str)
    {
        try
        {
            boost::lexical_cast<Url>(str);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
};

}

#endif // YT_URL_H_
