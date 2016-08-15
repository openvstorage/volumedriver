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

#include "ArakoonConfigFetcher.h"
#include "ConfigFetcher.h"
#include "EtcdConfigFetcher.h"
#include "FileConfigFetcher.h"

namespace youtils
{

std::unique_ptr<ConfigFetcher>
ConfigFetcher::create(const Uri& uri)
{
    if (ArakoonUrl::is_one(uri))
    {
        return std::make_unique<ArakoonConfigFetcher>(ArakoonUrl(uri));
    }
    else if (EtcdUrl::is_one(uri))
    {
        return std::make_unique<EtcdConfigFetcher>(EtcdUrl(uri));
    }
    else if (FileUrl::is_one(uri))
    {
        return std::make_unique<FileConfigFetcher>(FileUrl(uri));
    }
    else
    {
        LOG_ERROR("Unsupported URI " << uri);
        throw Exception("Unsupported URI");
    }
}

}
