// Copyright 2015 iNuron NV
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

#ifndef BACKEND_SIMPLE_FETCHER_H_
#define BACKEND_SIMPLE_FETCHER_H_

#include "BackendConnectionInterface.h"
#include "Namespace.h"

#include <boost/filesystem.hpp>

#include <youtils/FileDescriptor.h>
#include <youtils/Logging.h>

namespace backend
{

class SimpleFetcher
    : public BackendConnectionInterface::PartialReadFallbackFun
{
public:
    SimpleFetcher(BackendConnectionInterface& conn,
                  const Namespace& nspace,
                  const boost::filesystem::path& home);

    virtual ~SimpleFetcher();

    SimpleFetcher(const SimpleFetcher&) = delete;

    SimpleFetcher&
    operator=(const SimpleFetcher&) = delete;

    virtual youtils::FileDescriptor&
    operator()(const Namespace& ns,
               const std::string& object_name,
               InsistOnLatestVersion) override final;

private:
    DECLARE_LOGGER("SimpleFetcher");

    BackendConnectionInterface& conn_;
    const Namespace nspace_;
    const boost::filesystem::path home_;

    using FDMap = std::map<std::string,
                           std::unique_ptr<youtils::FileDescriptor>>;
    FDMap fd_map_;
};

}

#endif // !BACKEND_SIMPLE_FETCHER_H_
