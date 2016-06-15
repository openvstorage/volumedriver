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
