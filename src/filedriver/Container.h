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

#ifndef FILE_DRIVER_CONTAINER_H_
#define FILE_DRIVER_CONTAINER_H_

#include <vector>

#include <boost/thread/mutex.hpp>

#include <youtils/Logging.h>
#include <youtils/StrongTypedString.h>

#include <backend/BackendInterface.h>

STRONG_TYPED_STRING(filedriver, ContainerId);

namespace filedriver
{

class Extent;
class ExtentId;
class ExtentCache;

class Container
{
public:
    Container(const ContainerId& cid,
              std::shared_ptr<ExtentCache>& cache,
              std::shared_ptr<backend::BackendInterface>& bi);

    ~Container() = default;

    Container(const Container&) = delete;

    Container&
    operator=(const Container&) = delete;

    void
    restart();

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

    void
    drop_from_cache();

    uint64_t
    size() const
    {
        return size_;
    }

    const ContainerId id;

private:
    DECLARE_LOGGER("FileDriverContainer");

    mutable boost::mutex lock_;
    std::shared_ptr<ExtentCache> cache_;
    std::shared_ptr<backend::BackendInterface> bi_;

    // XXX: These have the potential of going out of sync with reality. Be smarter.
    uint64_t size_;
    std::vector<bool> extents_;

    bool
    extent_exists_(uint32_t off) const;

    void
    extent_exists_(uint32_t off, bool exists);

    void
    erase_extents_(bool from_backend);

    std::shared_ptr<Extent>
    new_extent_(const ExtentId& eid);

    std::shared_ptr<Extent>
    find_extent_(const ExtentId& eid);

    std::shared_ptr<Extent>
    find_or_create_extent_(const ExtentId& eid);
};

typedef std::shared_ptr<Container> ContainerPtr;

}

#endif // !FILE_DRIVER_CONTAINER_H_
