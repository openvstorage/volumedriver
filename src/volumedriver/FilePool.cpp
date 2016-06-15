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

#include "FilePool.h"
#include "youtils/UUID.h"
#include <youtils/IOException.h>
#include <boost/filesystem/fstream.hpp>
namespace volumedriver
{

FilePool::FilePool(const fs::path& directory)
 : directory_(directory / youtils::UUID().str())
{
    LOG_INFO("Creating new FilePool at " << directory_.string());
    if(fs::exists(directory_))
    {
        LOG_ERROR("Trying to create a FilePool where something already exists " << directory_);
        throw fungi::IOException("File Exists");
    }
    else
    {
        fs::create_directories(directory_);
    }
}

FilePool::~FilePool()
{
    LOG_INFO("Cleaning up FilePool at " << directory_.string());
    fs::remove_all(directory_);
}

fs::path
FilePool::newFile(const std::string& name)
{
    fs::path ret = directory_ / name;
    if(fs::exists(ret))
    {
        LOG_ERROR("Not creating an existing file as newfile in the File Pool");
        throw fungi::IOException("File Exists");
    }
    else
    {
        fs::ofstream r(ret,std::ios::out|std::ios::trunc);
        return ret;
    }
}

fs::path
FilePool::tempFile(const std::string name)
{
    return youtils::FileUtils::create_temp_file(directory_,
                                       name);

}
}
// Local Variables: **
// End: **
