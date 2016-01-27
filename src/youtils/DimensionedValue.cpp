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
#include "Catchers.h"
#include "DimensionedValue.h"

#include <sstream>
#include <cmath>
#include <limits>
#include <stdint.h>

namespace youtils
{

#define DPTN(val) const char* PT<Prefix :: val>::name= #val;

DPTN(B)
DPTN(KB)
DPTN(MB)
DPTN(GB)
DPTN(TB)
DPTN(PB)
DPTN(KiB)
DPTN(MiB)
DPTN(GiB)
DPTN(TiB)
DPTN(PiB)
#undef DPTN

namespace
{

DECLARE_LOGGER("DimensionedValueUtils");

const std::string&
getUnlimited()
{
    static const std::string unlimited("UNLIMITED");
    return unlimited;
}

}

DimensionedValue::DimensionedValue(const std::string& i)
{
    if (i == getUnlimited())
    {
        prefix_ = Prefix::Unlimited;
        value_ = 0;
        multiplier_ = 0;
    }
    else
    {
        std::stringstream ss(i);
        ss >> value_;
        if(not ss)
        {
            LOG_ERROR("Could not parse " << i << " as a uint64 ");
            throw NoParse(i);
        }

        std::string pref;
        ss >> pref;

        if (!FindPrefix<types>::name(pref,
                                     prefix_,
                                     multiplier_))
        {
            LOG_ERROR("Could not find a prefix in " << i);
            throw NoParse(i);
        }
        else if (value_ >= (std::numeric_limits<uint64_t>::max() / multiplier_))
        {
            uint64_t val = (std::numeric_limits<uint64_t>::max() / multiplier_);
            LOG_ERROR("Largest value representable is " << val << " " << pref);
            throw NoParse(i);
        }
    }
}

DimensionedValue::DimensionedValue(unsigned long long bytes)
{
    if (bytes == std::numeric_limits<uint64_t>::max())
    {
        prefix_ = Prefix::Unlimited;
        value_ = 0;
        multiplier_ = 0;
    }
    else
    {
        //probably unsafe
        //TODO fix
        std::pair<Prefix, uint64_t> t = FindBestPrefix<types>::fbp(bytes);

        prefix_ = t.first;
        multiplier_ = t.second;
        value_ = bytes / t.second;
    }
}

DimensionedValue::DimensionedValue()
    : DimensionedValue(0)
{
    TODO("AR: rather create an invalid value?");
}

std::string
DimensionedValue::toString() const
{
    //    std::stringstream s;
    switch(prefix_)
    {
    case Prefix::NotValid:
        return "NotValid";
    case Prefix::Unlimited:
        return getUnlimited();
    default:
        std::stringstream ss;
        ss << value_ << FindName<types>::getString(prefix_);
        return ss.str();
    }
}

unsigned long long
DimensionedValue::getBytes() const
{
    switch (prefix_)
    {
    case Prefix::Unlimited:
        return std::numeric_limits<uint64_t>::max();
    default:
        return multiplier_ * value_;
    }
}

bool
DimensionedValue::operator==(const DimensionedValue& other) const
{
    return prefix_ == other.prefix_ and
        value_ == other.value_ and
        multiplier_ == other.multiplier_;
}

std::istream&
operator>>(std::istream& is,
           DimensionedValue& v)
{
    std::string s;
    is >> s;

    if (is)
    {
        try
        {
            const DimensionedValue tmp(s);
            v = tmp;
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR("Failed to parse \"" << s << "\": " << EWHAT);
                is.setstate(std::ios_base::failbit);
            });
    }

    return is;
}

}

// Local Variables: **
// mode: c++ **
// End: **
