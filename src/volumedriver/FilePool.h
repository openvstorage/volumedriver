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

#ifndef FILEPOOL_H
#define FILEPOOL_H
#include "youtils/FileUtils.h"
#include <youtils/Logging.h>

namespace volumedriver
{
namespace fs = boost::filesystem;

class FilePool
{
public:
    explicit FilePool(const fs::path& directory);

    ~FilePool();

    fs::path
    newFile(const std::string&);

    const fs::path&
    directory() const
    {
        return directory_;
    }

    fs::path
    tempFile(const std::string name = "FILE_POOL_TEMP_FILE");

    DECLARE_LOGGER("FilePool");

private:
    const fs::path directory_;

};
}
#endif
// Local Variables: **
// End: **
