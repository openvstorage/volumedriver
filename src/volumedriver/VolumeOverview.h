// Copyright 2015 iNuron NV
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
