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
