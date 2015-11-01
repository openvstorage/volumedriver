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

#ifndef TLOG_TOOLCUT_H
#define TLOG_TOOLCUT_H
#include "../TLog.h"

namespace toolcut
{
namespace vd = volumedriver;

class TLogToolCut
{
public:
    explicit TLogToolCut(const vd::TLog& i_tlog);

    bool
    writtenToBackend() const;

    std::string
    getName() const;

    std::string
    getUUID() const;

    static bool
    isTLogString(const std::string& in);

    static std::string
    getUUIDFromTLogName(const std::string& tlogName);

    std::string
    str() const;

    std::string
    repr() const;

private:
    const vd::TLog tlog_;
};
}
#endif // TLOG_TOOLCUT_H

// Local Variables: **
// compile-command: "scons --kernel_version=system -D -j 4" **
// End: **
