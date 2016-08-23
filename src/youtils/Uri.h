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

#ifndef YT_URI_H_
#define YT_URI_H_

#include "Assert.h"
#include "Logging.h"
#include "Serialization.h"

#include <iosfwd>
#include <string>
#include <unordered_map>

#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>

namespace youtils
{

class Uri
{
public:
    explicit Uri(const std::string&);

    Uri() = default;

    ~Uri() = default;

    Uri(const Uri&) = default;

    Uri(Uri&&) = default;

    Uri&
    operator=(const Uri&) = default;

    Uri&
    operator=(Uri&&) = default;

    bool
    operator==(const Uri&) const;

    bool
    operator!=(const Uri& other) const
    {
        return not operator==(other);
    }

    bool
    operator<(const Uri& other) const;

    using Query = std::unordered_map<std::string, boost::optional<std::string>>;

#define URI_COMPONENT(typ, name)                    \
public:                                             \
    const typ&                                      \
    name() const                                    \
    {                                               \
        return name ## _;                           \
    }                                               \
                                                    \
    Uri&                                            \
    name(const typ& n)                              \
    {                                               \
        name ## _ = n;                              \
        return *this;                               \
    }                                               \
                                                    \
    Uri&                                            \
    name(typ&& n)                                   \
    {                                               \
        name ## _ = std::move(n);                   \
        return *this;                               \
    }                                               \
                                                    \
private:                                            \
    typ name ## _

    URI_COMPONENT(boost::optional<std::string>, scheme);
    URI_COMPONENT(boost::optional<std::string>, user_info);
    URI_COMPONENT(boost::optional<std::string>, host);
    URI_COMPONENT(boost::optional<uint16_t>, port);
    URI_COMPONENT(std::vector<std::string>, path_segments);
    URI_COMPONENT(Query, query);
    URI_COMPONENT(boost::optional<std::string>, fragment);
    URI_COMPONENT(bool, absolute_path) = false;

#undef URI_COMPONENT

public:
    std::string
    path() const;

private:
    DECLARE_LOGGER("Uri");

    friend class boost::serialization::access;

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<typename Archive>
    void
    load(Archive& ar,
         const unsigned version)
    {
        CHECK_VERSION(version, 1);

        auto s(boost::lexical_cast<std::string>(*this));
        ar & boost::serialization::make_nvp("uri",
                                            s);
        *this = Uri(s);
    }

    template<typename Archive>
    void
    save(Archive& ar,
         const unsigned version) const
    {
        CHECK_VERSION(version, 1);

        const auto s(boost::lexical_cast<std::string>(*this));
        ar & boost::serialization::make_nvp("uri",
                                            s);
    }
};

std::ostream&
operator<<(std::ostream&,
           const Uri&);

std::istream&
operator>>(std::istream&,
           Uri&);

}

BOOST_CLASS_VERSION(youtils::Uri, 1);

#endif // !YT_URI_H_
