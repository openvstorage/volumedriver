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

#ifndef YT_CONFIG_FETCHER_H_
#define YT_CONFIG_FETCHER_H_

#include "IOException.h"
#include "Logging.h"
#include "VolumeDriverComponent.h"

#include <boost/property_tree/ptree_fwd.hpp>

namespace youtils
{

class Uri;

struct ConfigFetcher
{
    MAKE_EXCEPTION(Exception, fungi::IOException);

    static std::unique_ptr<ConfigFetcher>
    create(const Uri&);

    virtual ~ConfigFetcher() = default;

    virtual boost::property_tree::ptree
    operator()(VerifyConfig) = 0;

private:
    DECLARE_LOGGER("ConfigFetcher");
};

}

#endif // !YT_CONFIG_FETCHER_H_
