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

#ifndef VD_RELOCATION_READER_FACTORY_H_
#define VD_RELOCATION_READER_FACTORY_H_

#include "CombinedTLogReader.h"

#include <boost/filesystem.hpp>

#include <youtils/Logging.h>
#include <youtils/ScopeExit.h>

#include <backend/BackendInterface.h>

namespace volumedriver
{

class RelocationReaderFactory
{
public:
    RelocationReaderFactory(std::vector<std::string> relocation_logs,
                            const boost::filesystem::path& tlog_location,
                            backend::BackendInterfacePtr bi,
                            const CombinedTLogReader::FetchStrategy fetch_strategy);

    ~RelocationReaderFactory() = default;

    RelocationReaderFactory(const RelocationReaderFactory&) = default;

    RelocationReaderFactory&
    operator=(const RelocationReaderFactory&) = default;

    void
    prepare_one();

    std::unique_ptr<TLogReaderInterface>
    get_one();

    const std::vector<std::string>&
    relocation_logs() const
    {
        return relocation_logs_;
    }

private:
    DECLARE_LOGGER("RelocationReaderFactory");

    const std::vector<std::string> relocation_logs_;
    const boost::filesystem::path tlog_location_;
    backend::BackendInterfacePtr bi_;
    const CombinedTLogReader::FetchStrategy fetch_strategy_;

    std::unique_ptr<TLogReaderInterface> stock_;
};

}

#endif // VD_RELOCATION_READER_FACTORY_H_
