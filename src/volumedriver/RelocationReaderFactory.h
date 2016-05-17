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
