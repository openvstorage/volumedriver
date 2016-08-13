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

#ifndef YT_ETCD_CONFIG_FETCHER_H_
#define YT_ETCD_CONFIG_FETCHER_H_

#include "ConfigFetcher.h"
#include "EtcdUrl.h"
#include "Logging.h"

namespace youtils
{

class EtcdConfigFetcher
    : public ConfigFetcher
{
public:
    explicit EtcdConfigFetcher(const EtcdUrl& url)
        : url_(url)
    {}

    virtual ~EtcdConfigFetcher() = default;

    EtcdConfigFetcher(const EtcdConfigFetcher&) = delete;

    EtcdConfigFetcher&
    operator=(const EtcdConfigFetcher&) = delete;

    virtual boost::property_tree::ptree
    operator()(VerifyConfig) override final;

private:
    DECLARE_LOGGER("EtcdConfigFetcher");

    const EtcdUrl url_;
};

}

#endif // !YT_ETCD_CONFIG_FETCHER_H_
