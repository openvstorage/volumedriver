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

#ifndef YT_ARAKOON_CONFIG_FETCHER_H_
#define YT_ARAKOON_CONFIG_FETCHER_H_

#include "ConfigFetcher.h"
#include "ArakoonUrl.h"
#include "Logging.h"

namespace youtils
{

class ArakoonConfigFetcher
    : public ConfigFetcher
{
public:
    explicit ArakoonConfigFetcher(const ArakoonUrl& url)
        : url_(url)
    {}

    virtual ~ArakoonConfigFetcher() = default;

    ArakoonConfigFetcher(const ArakoonConfigFetcher&) = delete;

    ArakoonConfigFetcher&
    operator=(const ArakoonConfigFetcher&) = delete;

    virtual boost::property_tree::ptree
    operator()(VerifyConfig) override final;

private:
    DECLARE_LOGGER("ArakoonConfigFetcher");

    const ArakoonUrl url_;
};

}

#endif // !YT_ARAKOON_CONFIG_FETCHER_H_
