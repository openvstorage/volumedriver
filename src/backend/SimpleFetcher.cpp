// Copyright 2015 Open vStorage NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
