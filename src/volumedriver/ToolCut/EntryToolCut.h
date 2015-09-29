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

#ifndef ENTRY_TOOLCUT_H_
#define ENTRY_TOOLCUT_H_
#include "../Entry.h"
#include "ClusterLocationToolCut.h"
#include <youtils/Logging.h>
namespace toolcut
{
class EntryToolCut
{
    DECLARE_LOGGER("EntryToolCut")
public:
    EntryToolCut(const volumedriver::Entry* entry)
        :entry_(entry)
    {
        VERIFY(entry_);
    }

    volumedriver::Entry::Type
    type() const;

    volumedriver::CheckSum::value_type
    getCheckSum() const;

    volumedriver::ClusterAddress
    clusterAddress() const;

    ClusterLocationToolCut
    clusterLocation() const;

    std::string
    str() const;

    std::string
    repr() const;

private:
    const volumedriver::Entry* entry_;
};

}

#endif

// Local Variables: **
// mode: c++ **
// End: **
