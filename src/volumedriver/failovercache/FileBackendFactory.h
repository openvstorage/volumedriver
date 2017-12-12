// Copyright (C) 2017 iNuron NV
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

#ifndef DTL_FILE_BACKEND_FACTORY_H_
#define DTL_FILE_BACKEND_FACTORY_H_

#include "FileBackend.h"

#include <boost/filesystem/path.hpp>

#include <youtils/Logging.h>

namespace youtils
{
class FileDescriptor;
}

namespace volumedriver
{

namespace failovercache
{

class FileBackendFactory
{
public:
    explicit FileBackendFactory(const FileBackend::Config&);

    ~FileBackendFactory();

    FileBackendFactory(const FileBackendFactory&) = delete;

    FileBackendFactory&
    operator=(const FileBackendFactory&) = delete;

    FileBackendFactory(FileBackendFactory&&) = default;

    FileBackendFactory&
    operator=(FileBackendFactory&&) = default;

    std::unique_ptr<FileBackend>
    make_one(const std::string& nspace,
             const volumedriver::ClusterSize) const;

private:
    DECLARE_LOGGER("DtlFileBackendFactory");

    FileBackend::Config config_;
    std::unique_ptr<youtils::FileDescriptor> fd_;
    boost::unique_lock<youtils::FileDescriptor> lock_;
};

}

}

#endif // !DTL_FILE_BACKEND_FACTORY_H_
