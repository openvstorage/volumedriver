// Copyright (C) 2017 iNuron NV
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

#ifndef BACKEND_SWITCH_CONNECTION_POOL_POLICY_H_
#define BACKEND_SWITCH_CONNECTION_POOL_POLICY_H_

#include <iosfwd>

namespace backend
{

enum class SwitchConnectionPoolPolicy
{
    OnError,
    RoundRobin,
};

std::ostream&
operator<<(std::ostream&,
           SwitchConnectionPoolPolicy);

std::istream&
operator>>(std::istream&,
           SwitchConnectionPoolPolicy&);

enum SwitchConnectionPoolOnErrorPolicy
{
    OnBackendError = 1U << 0,
    OnTimeout = 1U << 1,
};

std::ostream&
operator<<(std::ostream&,
           SwitchConnectionPoolOnErrorPolicy);

std::istream&
operator>>(std::istream&,
           SwitchConnectionPoolOnErrorPolicy&);

}

#endif // !BACKEND_SWITCH_CONNECTION_POOL_POLICY_H_
