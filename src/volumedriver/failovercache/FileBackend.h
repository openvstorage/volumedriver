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

#ifndef DTL_FILE_BACKEND_H_
#define DTL_FILE_BACKEND_H_

#include "Backend.h"
#include "fungilib/File.h"

#include <memory>

#include <boost/filesystem.hpp>

namespace volumedriver
{

namespace failovercache
{

class FileBackend
    : public Backend
{
public:
    FileBackend(const boost::filesystem::path&,
                const std::string&,
                const volumedriver::ClusterSize,
                const boost::optional<size_t> stream_buffer_size);

    ~FileBackend();

    FileBackend(const FileBackend&) = delete;

    FileBackend&
    operator=(const FileBackend&) = delete;

    virtual void
    open(const volumedriver::SCO) override final;

    virtual void
    close() override final;

    virtual void
    add_entries(std::vector<volumedriver::FailOverCacheEntry>,
                std::unique_ptr<uint8_t[]>) override final;

    virtual void
    flush() override final;

    virtual void
    remove(const volumedriver::SCO) override final;

    virtual void
    get_entries(const volumedriver::SCO,
                Backend::EntryProcessorFun&) override final;

    const boost::filesystem::path&
    root() const
    {
        return root_;
    }

    static size_t
    default_stream_buffer_size();

private:
    DECLARE_LOGGER("DtlFileBackend");

    std::unique_ptr<fungi::File> file_;
    const boost::filesystem::path root_;
    const size_t stream_buffer_size_;

    boost::filesystem::path
    make_path_(const volumedriver::SCO) const;
};

}

}

#endif // !DTL_FILE_BACKEND_H_
