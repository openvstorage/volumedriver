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
