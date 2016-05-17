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

#include "SimpleFetcher.h"

#include <youtils/FileUtils.h>

namespace backend
{

namespace bs = boost::system;
namespace fs = boost::filesystem;
namespace yt = youtils;

SimpleFetcher::SimpleFetcher(BackendConnectionInterface& conn,
                             const Namespace& nspace,
                             const fs::path& home)
    : conn_(conn)
    , nspace_(nspace)
    , home_(home)
{}

SimpleFetcher::~SimpleFetcher()
{
    for (const auto& p : fd_map_)
    {
        bs::error_code ec;
        fs::remove_all(p.second->path(),
                       ec);
        if (ec)
        {
            LOG_ERROR("Failed to remove " << p.second->path() << ": " << ec.message());
        }
    }
}

yt::FileDescriptor&
SimpleFetcher::operator()(const Namespace& nspace,
                          const std::string& obj_name,
                          InsistOnLatestVersion insist_on_latest)
{
    VERIFY(nspace_ == nspace);

    if (fd_map_.count(obj_name) == 0)
    {
        fs::path p(yt::FileUtils::create_temp_file(home_,
                                                   obj_name));
        conn_.read(nspace,
                   p,
                   obj_name,
                   insist_on_latest);

        fd_map_[obj_name] = std::make_unique<yt::FileDescriptor>(p,
                                                                 yt::FDMode::Read,
                                                                 CreateIfNecessary::F);
    }

    return *fd_map_.at(obj_name);
}

}
