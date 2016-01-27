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
