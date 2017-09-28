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

#include "SwitchConnectionPoolPolicy.h"

#include <iostream>

#include <boost/bimap.hpp>

#include <youtils/StreamUtils.h>

namespace backend
{

namespace yt = youtils;

namespace
{

using PolicyTranslationsMap = boost::bimap<SwitchConnectionPoolPolicy, std::string>;

const PolicyTranslationsMap&
policy_translations()
{
    static const std::vector<PolicyTranslationsMap::value_type> vec{
        { SwitchConnectionPoolPolicy::OnError, "SwitchOnError" },
        { SwitchConnectionPoolPolicy::RoundRobin, "RoundRobin" },
    };

    static const PolicyTranslationsMap map(vec.begin(),
                                           vec.end());

    return map;
}

using ErrorPolicyTranslationsMap = boost::bimap<SwitchConnectionPoolOnErrorPolicy, std::string>;

const ErrorPolicyTranslationsMap&
error_policy_translations()
{
    static const std::vector<ErrorPolicyTranslationsMap::value_type> vec{
        { SwitchConnectionPoolOnErrorPolicy::OnBackendError, "OnBackendError" },
        { SwitchConnectionPoolOnErrorPolicy::OnTimeout, "OnTimeout" },
    };

    static const ErrorPolicyTranslationsMap map(vec.begin(),
                                                vec.end());

    return map;
}

}

std::ostream&
operator<<(std::ostream& os,
           const SwitchConnectionPoolPolicy s)
{
    return yt::StreamUtils::stream_out(policy_translations().left,
                                       os,
                                       s);
}

std::istream&
operator>>(std::istream& is,
           SwitchConnectionPoolPolicy& s)
{
    return yt::StreamUtils::stream_in(policy_translations().right,
                                      is,
                                      s);
}

std::ostream&
operator<<(std::ostream& os,
           const SwitchConnectionPoolOnErrorPolicy s)
{
    return yt::StreamUtils::stream_out(error_policy_translations().left,
                                       os,
                                       s);
}

std::istream&
operator>>(std::istream& is,
           SwitchConnectionPoolOnErrorPolicy& s)
{
    return yt::StreamUtils::stream_in(error_policy_translations().right,
                                      is,
                                      s);
}

}
