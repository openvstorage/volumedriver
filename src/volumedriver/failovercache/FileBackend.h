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

#ifndef DTL_FILE_BACKEND_H_
#define DTL_FILE_BACKEND_H_

#include "FailOverCacheWriter.h"
#include "fungilib/File.h"

#include <memory>

#include <boost/filesystem.hpp>

namespace failovercache
{

class FileBackend
    : public FailOverCacheWriter
{
public:
    FileBackend(const boost::filesystem::path&,
                const std::string&,
                const volumedriver::ClusterSize);

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
                FailOverCacheWriter::EntryProcessorFun&) override final;

private:
    DECLARE_LOGGER("DtlFileBackend");

    std::unique_ptr<fungi::File> file_;
    const boost::filesystem::path root_;

    boost::filesystem::path
    make_path_(const volumedriver::SCO) const;
};

}

#endif // !DTL_FILE_BACKEND_H_
