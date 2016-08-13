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

#ifndef YT_FILE_CONFIG_FETCHER_H_
#define YT_FILE_CONFIG_FETCHER_H_

#include "ConfigFetcher.h"
#include "FileUrl.h"
#include "Logging.h"

namespace youtils
{

class FileConfigFetcher
    : public ConfigFetcher
{
public:
    explicit FileConfigFetcher(const FileUrl& url)
        : url_(url)
    {}

    virtual ~FileConfigFetcher() = default;

    FileConfigFetcher(const FileConfigFetcher&) = delete;

    FileConfigFetcher&
    operator=(const FileConfigFetcher&) = delete;

    virtual boost::property_tree::ptree
    operator()(VerifyConfig verify_config) override final
    {
        return VolumeDriverComponent::read_config_file(url_.path(),
                                                       verify_config);
    }

private:
    DECLARE_LOGGER("FileConfigFetcher");

    const FileUrl url_;
};

}

#endif // !YT_FILE_CONFIG_FETCHER_H_
