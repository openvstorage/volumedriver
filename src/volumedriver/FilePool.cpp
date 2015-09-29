// Copyright 2015 Open vStorage NV
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
