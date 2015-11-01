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

#ifndef FILE_DRIVER_EXTENT_H_
#define FILE_DRIVER_EXTENT_H_

#include <boost/filesystem.hpp>

#include <youtils/Logging.h>
#include <youtils/StrongTypedString.h>

namespace filedriver
{

class Extent
{
public:
    explicit Extent(const boost::filesystem::path& p);

    ~Extent() = default;

    Extent(const Extent&) = delete;

    Extent&
    operator=(const Extent&) = delete;

    static uint64_t
    capacity()
    {
        return 1ULL << 20; // 1 MiB by default.
    }

    size_t
    size();

    size_t
    read(off_t off,
         void* buf,
         size_t bufsize);

    size_t
    write(off_t off,
          const void* buf,
          size_t bufsize);

    void
    resize(uint64_t size);

    void
    unlink();

    const boost::filesystem::path path;

private:
    DECLARE_LOGGER("FileDriverExtent");
};

}

#endif // !FILE_DRIVER_EXTENT_H_
