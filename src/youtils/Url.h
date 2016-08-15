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
#include "Uri.h"

#include <iosfwd>
#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/regex.hpp>

namespace youtils
{

template<typename UrlTraits>
class Url
{
public:
    explicit Url(const Uri& uri)
        : uri_(uri)
    {
        if (uri.scheme() != UrlTraits::scheme())
        {
            throw std::invalid_argument("URI is not an URL of the expected type");
        }

        if (not uri_.port())
        {
            uri_.port(UrlTraits::default_port());
        }
    }

    explicit Url(const std::string& s)
        : Url(Uri(s))
    {}

    Url()
    {
        uri_.scheme(UrlTraits::scheme()).port(UrlTraits::default_port());
    }

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
        return uri_ == other.uri_;
    }

    bool
    operator!=(const Url& other) const
    {
        return not operator==(other);
    }

    std::string
    host() const
    {
        VERIFY(uri_.host());
        return *uri_.host();
    }

    const boost::optional<uint16_t>&
    port() const
    {
        return uri_.port();
    }

    std::string
    path() const
    {
        return uri_.path();
    }

    const Uri::Query&
    query() const
    {
        return uri_.query();
    }

    const Uri&
    uri() const
    {
        return uri_;
    }

    friend std::ostream&
    operator<<(std::ostream& os,
               const Url& url)
    {
        return os << url.uri_;
    }

    friend std::istream&
    operator>>(std::istream& is,
               Url& url)
    {
        Uri uri;
        is >> uri;

        try
        {
            url = Url(uri);
        }
        catch (...)
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
            return is_one(Uri(str));
        }
        catch (...)
        {
            return false;
        }
    }

    static bool
    is_one(const Uri& uri)
    {
        return uri.scheme() == UrlTraits::scheme();
    }

private:
    DECLARE_LOGGER("Uri");

    Uri uri_;
};

}

#endif // YT_URL_H_
