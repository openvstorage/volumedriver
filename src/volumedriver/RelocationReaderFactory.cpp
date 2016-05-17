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

#include "RelocationReaderFactory.h"
#include "TLogReaderInterface.h"

namespace volumedriver
{

namespace be = backend;
namespace fs = boost::filesystem;
namespace yt = youtils;

class RelocationReaderFromTempDir
    : public TLogReaderInterface
{
public:
    RelocationReaderFromTempDir(const fs::path& path,
                                const std::vector<std::string>& relocs,
                                be::BackendInterfacePtr bi,
                                const CombinedTLogReader::FetchStrategy strategy)
        : path_(make_dir_(path / yt::UUID().str()))
        , treader_(CombinedTLogReader::create(path_,
                                              relocs,
                                              std::move(bi),
                                              strategy))
    {}

    ~RelocationReaderFromTempDir()
    {
        try
        {
            fs::remove_all(path_);
        }
        CATCH_STD_ALL_LOG_IGNORE("Failed to remove " << path_);
    }

    virtual const Entry*
    nextAny() override final
    {
        return treader_->nextAny();
    }

private:
    DECLARE_LOGGER("RelocationReaderFromTempDir");

    const fs::path path_;
    TLogReaderInterface::Ptr treader_;

    static const fs::path&
    make_dir_(const fs::path& path)
    {
        fs::create_directories(path);
        return path;
    }
};

RelocationReaderFactory::RelocationReaderFactory(std::vector<std::string> relocation_logs,
                                                 const fs::path& tlog_location,
                                                 backend::BackendInterfacePtr bi,
                                                 const CombinedTLogReader::FetchStrategy fetch_strategy)
    : relocation_logs_(std::move(relocation_logs))
    , tlog_location_(tlog_location)
    , bi_(std::move(bi))
    , fetch_strategy_(fetch_strategy)
{}

void
RelocationReaderFactory::prepare_one()
{
    if (not stock_)
    {
        stock_ = std::make_unique<RelocationReaderFromTempDir>(tlog_location_,
                                                               relocation_logs_,
                                                               bi_->clone(),
                                                               fetch_strategy_);
    }
}

std::unique_ptr<TLogReaderInterface>
RelocationReaderFactory::get_one()
{
    prepare_one();
    return std::move(stock_);
}

}
