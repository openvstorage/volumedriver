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
