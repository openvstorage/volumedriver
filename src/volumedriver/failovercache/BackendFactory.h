// Copyright 2016 iNuron NV
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

#include "Backend.h"

#include <boost/filesystem.hpp>

#include <youtils/FileDescriptor.h>
#include <youtils/Logging.h>

namespace failovercache
{

class BackendFactory
{
public:
    explicit BackendFactory(const boost::optional<boost::filesystem::path>&);

    ~BackendFactory();

    BackendFactory(const BackendFactory&) = delete;

    BackendFactory&
    operator=(const BackendFactory&) = delete;

    std::unique_ptr<Backend>
    make_backend(const std::string&,
                 const volumedriver::ClusterSize);

private:
    DECLARE_LOGGER("DtlBackendFactory");

    std::unique_ptr<youtils::FileDescriptor> lock_fd_;
    boost::unique_lock<youtils::FileDescriptor> file_lock_;

    const boost::optional<boost::filesystem::path> root_;
};

}
