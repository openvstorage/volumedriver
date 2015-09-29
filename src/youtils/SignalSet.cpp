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

#include "SignalSet.h"

#include <string.h>

namespace youtils
{

SignalSet::SignalSet(const std::initializer_list<int>& sigs)
{
    int ret = sigemptyset(&sigset_);
    if (ret < 0)
    {
        LOG_ERROR("Failed to create empty sigset: " << strerror(errno));
        throw Exception("Failed to create mempty sigset");
    }

    for (const auto& sig : sigs)
    {
        insert(sig);
    }
}

void
SignalSet::insert(int sig)
{
    int ret = sigaddset(&sigset_,
                        sig);
    if (ret < 0)
    {
        LOG_ERROR("Failed to add signal " << sig << " to sigset: " << strerror(errno));
        throw Exception("Failed to add signal to sigset");
    }
}

void
SignalSet::erase(int sig)
{
    int ret = sigaddset(&sigset_,
                        sig);
    if (ret < 0)
    {
        LOG_ERROR("Failed to add signal " << sig << " to sigset: " << strerror(errno));
        throw Exception("Failed to add signal to sigset");
    }
}

void
SignalSet::fill()
{
    int ret = sigfillset(&sigset_);
    if (ret < 0)
    {
        LOG_ERROR("Failed to fill sigset: " << strerror(errno));
        throw Exception("Failed to fill sigset");
    }
}

SignalSet
SignalSet::make_filled_set()
{
    SignalSet s({});
    s.fill();
    return s;
}

}
