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

#ifndef VOLUME_OVERVIEW_H_
#define VOLUME_OVERVIEW_H_
#include "Types.h"
namespace volumedriver
{
struct VolumeOverview
{
    enum class VolumeState
    {
        Active,
        Restarting
    };

    VolumeOverview(VolumeOverview&& v)
        : ns_(std::move(v.ns_))
        , state_(v.state_)
    {}

    VolumeOverview(Namespace ns,
                   VolumeState state)
        : ns_(ns)
        , state_(state)
    {}

    Namespace ns_;
    VolumeState state_;

    static const char*
    getStateString(const VolumeState& state)
    {
        switch(state)
        {
        case VolumeState::Active:
            return "Active";
        case VolumeState::Restarting:
            return "Restarting";
        }
        UNREACHABLE;
    }
};

}

#endif // VOLUME_OVERVIEW_H_

// Local Variables: **
// mode: c++ **
// End: **
