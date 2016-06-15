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

#include "VolumeFailOverState.h"

#include <iostream>
#include <vector>

#include <boost/bimap.hpp>

#include <youtils/Assert.h>
#include <youtils/Logging.h>

namespace volumedriver
{

namespace
{

DECLARE_LOGGER("VolumeFailOverState");

// Hack around boost::bimap not supporting initializer lists by building a vector first
// and then filling the bimap from it. And since we don't want the vector to stick around
// forever we use a function.
typedef boost::bimap<VolumeFailOverState, std::string> TranslationsMap;

TranslationsMap
init_translations()
{
    const std::vector<TranslationsMap::value_type> initv{
        { VolumeFailOverState::OK_SYNC, "OK_SYNC" },
        { VolumeFailOverState::OK_STANDALONE, "OK_STANDALONE" },
        { VolumeFailOverState::KETCHUP, "CATCHUP" },
        { VolumeFailOverState::DEGRADED, "DEGRADED" }
    };

    return TranslationsMap(initv.begin(), initv.end());
};

const TranslationsMap translations(init_translations());

void
reminder(VolumeFailOverState st) __attribute__((unused));

void
reminder(VolumeFailOverState st)
{
    // If the compiler yells at you here chances are that you also forgot to update
    // the above translations. DO IT NOW!

    switch (st)
    {
    case VolumeFailOverState::OK_SYNC:
    case VolumeFailOverState::OK_STANDALONE:
    case VolumeFailOverState::KETCHUP:
    case VolumeFailOverState::DEGRADED:
        break;
    }
};

}

const std::string&
volumeFailoverStateToString(VolumeFailOverState st)
{
    const auto it = translations.left.find(st);
    VERIFY(it != translations.left.end());

    return it->second;
}

std::ostream&
operator<<(std::ostream& os,
           const VolumeFailOverState st)
{
    const auto it = translations.left.find(st);
    if (it != translations.left.end())
    {
        os << it->second;
    }
    else
    {
        VERIFY(0 == "forgot to add a translation, didn't you?");
        os.setstate(std::ios_base::failbit);
    }

    return os;
}

std::istream&
operator>>(std::istream& is,
           VolumeFailOverState& st)
{
    std::string str;
    is >> str;

    const auto it = translations.right.find(str);
    if (it != translations.right.end())
    {
        st = it->second;
    }
    else
    {
        is.setstate(std::ios_base::failbit);
    }

    return is;
}

}
