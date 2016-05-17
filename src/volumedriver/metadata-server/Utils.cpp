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

#include <iostream>

#include <boost/bimap.hpp>

#include <youtils/Logging.h>
#include <youtils/StreamUtils.h>

namespace metadata_server
{

DECLARE_LOGGER("MetaDataServerUtils");

using namespace std::string_literals;

namespace yt = youtils;
namespace mdsproto = metadata_server_protocol;

namespace
{

void
role_reminder(Role t) __attribute__((unused));

void
role_reminder(Role t)
{
    switch (t)
    {
    case Role::Master:
    case Role::Slave:
        // If the compiler yells at you that you've forgotten dealing with an enum
        // value chances are that it's also missing from the translations map below.
        // If so add it RIGHT NOW.
        break;
    }
}

typedef boost::bimap<Role, std::string> TranslationsMap;

// Hack around boost::bimap not supporting initializer lists by building a vector first
// and then filling the bimap from it. And since we don't want the vector to stick around
// forever we use a function.
TranslationsMap
init_translations()
{
    const std::vector<TranslationsMap::value_type> initv{
        { Role::Master, "MASTER"s },
        { Role::Slave, "SLAVE"s }
    };

    return TranslationsMap(initv.begin(),
                           initv.end());
}

}

std::ostream&
operator<<(std::ostream& os,
           Role t)
{
    static const TranslationsMap translations(init_translations());
    return yt::StreamUtils::stream_out(translations.left,
                                       os,
                                       t);
}

std::istream&
operator>>(std::istream& is,
           Role& t)
{
    static const TranslationsMap translations(init_translations());
    return yt::StreamUtils::stream_in(translations.right,
                                      is,
                                      t);
}

Role
translate_role(mdsproto::Role role)
{
    switch (role)
    {
    case mdsproto::Role::MASTER:
        return Role::Master;
    case mdsproto::Role::SLAVE:
        return Role::Slave;
    }

    VERIFY(0 == "impossible code path reached");
}

mdsproto::Role
translate_role(Role role)
{
    switch (role)
    {
    case Role::Master:
        return mdsproto::Role::MASTER;
    case Role::Slave:
        return mdsproto::Role::SLAVE;
    }

    VERIFY(0 == "impossible code path reached");
}

}
