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

#include "VolumeFailoverState.h"

#include <iostream>
#include <vector>

#include <boost/bimap.hpp>

#include <youtils/Assert.h>
#include <youtils/Logging.h>

namespace volumedriver
{

namespace
{

DECLARE_LOGGER("VolumeFailoverState");

// Hack around boost::bimap not supporting initializer lists by building a vector first
// and then filling the bimap from it. And since we don't want the vector to stick around
// forever we use a function.
typedef boost::bimap<VolumeFailoverState, std::string> TranslationsMap;

TranslationsMap
init_translations()
{
    const std::vector<TranslationsMap::value_type> initv{
        { VolumeFailoverState::OK_SYNC, "OK_SYNC" },
        { VolumeFailoverState::OK_STANDALONE, "OK_STANDALONE" },
        { VolumeFailoverState::KETCHUP, "CATCHUP" },
        { VolumeFailoverState::DEGRADED, "DEGRADED" }
    };

    return TranslationsMap(initv.begin(), initv.end());
};

const TranslationsMap translations(init_translations());

void
reminder(VolumeFailoverState st) __attribute__((unused));

void
reminder(VolumeFailoverState st)
{
    // If the compiler yells at you here chances are that you also forgot to update
    // the above translations. DO IT NOW!

    switch (st)
    {
    case VolumeFailoverState::OK_SYNC:
    case VolumeFailoverState::OK_STANDALONE:
    case VolumeFailoverState::KETCHUP:
    case VolumeFailoverState::DEGRADED:
        break;
    }
};

}

const std::string&
volumeFailoverStateToString(VolumeFailoverState st)
{
    const auto it = translations.left.find(st);
    VERIFY(it != translations.left.end());

    return it->second;
}

std::ostream&
operator<<(std::ostream& os,
           const VolumeFailoverState st)
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
           VolumeFailoverState& st)
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
