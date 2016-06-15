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
