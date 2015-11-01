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

#include "Container.h"
#include "Extent.h"
#include "ExtentCache.h"
#include "ExtentId.h"

#include <boost/filesystem.hpp>

#include <youtils/Catchers.h>

#include <backend/BackendException.h>

namespace filedriver
{

namespace be = backend;
namespace fs = boost::filesystem;

#define LOCK()                                  \
    boost::lock_guard<decltype(lock_)> lg__(lock_)

Container::Container(const ContainerId& cid,
                     std::shared_ptr<ExtentCache>& cache,
                     std::shared_ptr<backend::BackendInterface>& bi)
    : id(cid)
    , cache_(cache)
    , bi_(bi)
    , size_(0)
{}

bool
Container::extent_exists_(uint32_t off) const
{
    const bool res = extents_.size() > off and extents_[off];
    LOG_TRACE(id << ": off " << off << ": " << res);

    return res;
}

void
Container::extent_exists_(uint32_t off, bool exists)
{
    LOG_TRACE(id << ": off " << off << ", exists " << exists);

    if (extents_.size() <= off)
    {
        extents_.resize(off + 1, false);
    }

    extents_[off] = exists;
}

std::shared_ptr<Extent>
Container::find_extent_(const ExtentId& eid)
{
    LOG_TRACE(eid);
    std::shared_ptr<Extent>
        ext(cache_->find(eid,
                         [&](const ExtentId& eid,
                             const fs::path& p) -> std::unique_ptr<Extent>
                         {
                             LOG_INFO(id << ": fetching extent " << eid << " to " << p);

                             try
                             {
                                 bi_->read(p, eid.str(), InsistOnLatestVersion::T);
                                 return std::unique_ptr<Extent>(new Extent(p));
                             }
                             catch (be::BackendObjectDoesNotExistException&)
                             {
                                 LOG_WARN("extent " << eid << " does not exist");
                                 return nullptr;
                             }
                         }));
    if (ext == nullptr)
    {
        LOG_ERROR("extent " << eid << " not present");
        throw fungi::IOException("Extent not present",
                                 eid.str().c_str());
    }

    return ext;
}

std::shared_ptr<Extent>
Container::new_extent_(const ExtentId& eid)
{
    LOG_INFO(id << ": creating new extent " << eid);

    std::shared_ptr<Extent>
        ext(cache_->find(eid,
                         [&](const ExtentId&,
                             const fs::path& p)
                         {
                             return std::unique_ptr<Extent>(new Extent(p));
                         }));

    VERIFY(ext != nullptr);
    return ext;
}

std::shared_ptr<Extent>
Container::find_or_create_extent_(const ExtentId& eid)
{
    LOG_TRACE(eid);

    if (extent_exists_(eid.offset))
    {
        return find_extent_(eid);
    }
    else
    {
        return new_extent_(eid);
    }
}

size_t
Container::read(off_t off,
                void* buf,
                size_t bufsize)
{
    LOCK();

    LOG_TRACE(id << ": off " << off << ", size " << bufsize);

    size_t res = 0;
    const size_t to_read = std::min(off - size() > 0 ? size() - off : 0,
                                    bufsize);

    while (res != to_read)
    {
        const uint32_t idx = off / Extent::capacity();
        const off_t eoff = off % Extent::capacity();
        const uint32_t r = std::min(Extent::capacity() - eoff, to_read - res);

        if (extent_exists_(idx))
        {
            const ExtentId eid(id, idx);
            std::shared_ptr<Extent> ext(find_extent_(eid));
            const size_t r2 = ext->read(eoff, buf, r);
            if (r2 != r)
            {
                memset(static_cast<uint8_t*>(buf) + r2, 0x0, r - r2);
            }
        }
        else
        {
            memset(buf, 0x0, r);
        }

        res += r;
        off += r;
        buf = static_cast<uint8_t*>(buf) + r;
    }

    return res;
}

size_t
Container::write(off_t off,
                 const void* buf,
                 size_t bufsize)
{
    LOCK();

    LOG_TRACE(id << ": off " << off << ", size " << bufsize);
    size_t res = 0;

    while (res != bufsize)
    {
        const uint32_t idx = off / Extent::capacity();
        const ExtentId eid(id, idx);
        const off_t eoff = off % Extent::capacity();
        const uint32_t r = std::min(Extent::capacity() - eoff, bufsize - res);

        std::shared_ptr<Extent> ext(find_or_create_extent_(eid));

        try
        {
            const size_t r2 = ext->write(eoff, buf, r);
            if (r2 != r)
            {
                LOG_ERROR(eid << ": wrote less (" << r2 << ") than expected (" << r << ")");
                throw fungi::IOException("Wrote less than expected to extent",
                                         eid.str().c_str(),
                                     EIO);
            }

            bi_->write(ext->path, eid.str(), OverwriteObject::T);
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR("Failed to write to extent " << eid << ": " << EWHAT);
                if (not extent_exists_(idx))
                {
                    // The extent was newly created and does not exist on the backend yet.
                    // Don't forget to release it before erasing it from the cache.
                    ext = nullptr;
                    cache_->erase(eid);
                }
                throw;
            });

        extent_exists_(idx, true);
        off += r;
        res += r;
        buf = static_cast<const uint8_t*>(buf) + r;

        if (off - size_ > 0)
        {
            size_ = off;
        }
    }

    return bufsize;
}

void
Container::resize(uint64_t size)
{
    LOCK();

    LOG_TRACE(id << ": size " << size);

    uint32_t idx = size / Extent::capacity();
    uint32_t eoff = size % Extent::capacity();
    const uint32_t rm_idx = eoff ? idx + 1 : idx;

    // cases:
    // * size = 0 -> all extents need to be removed - idx = 0, rm_idx = 0, eoff = 0
    // * size % Extent::capacity == 0 -> idx - 1 needs to be resized to Extent::capacity(),
    //   remove all extents >= idx
    // * otherwise: idx needs to be resized to size % Extent::capacity(), extents > idx
    //   need to be removed

    for (int i = extents_.size() - 1; i >= (int)rm_idx; --i)
    {
        if (extent_exists_(i))
        {
            ExtentId eid(id, i);
            cache_->erase(eid);
            bi_->remove(eid.str());
            extents_.resize(i);
        }

        if (size_ % Extent::capacity() != 0)
        {
            size_ -= size_ % Extent::capacity();
        }
        else
        {
            size_ -= Extent::capacity();
        }
    }

    if (eoff == 0 and
        idx > 0)
    {
        idx--;
        eoff = Extent::capacity();
    }

    if (eoff != 0)
    {
        ExtentId eid(id, idx);
        std::shared_ptr<Extent> ext(find_or_create_extent_(eid));

        try
        {
            ext->resize(eoff);
            bi_->write(ext->path, eid.str(), OverwriteObject::T);
            extent_exists_(idx, true);
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR(eid << ": failed to resize to " << eoff << ": " << EWHAT);
                if (not extent_exists_(idx))
                {
                    // The extent was newly created and does not exist on the backend yet.
                    // Don't forget to release it before erasing it from the cache.
                    ext = nullptr;
                    cache_->erase(eid);
                }
                throw;
            });
    }

    VERIFY(size == idx * Extent::capacity() + eoff);
    size_ = size;
}

void
Container::erase_extents_(bool from_backend)
{
    for (uint32_t i = 0; i < extents_.size(); ++i)
    {
        if (extent_exists_(i))
        {
            const ExtentId eid(id, i);

            cache_->erase(eid);

            if (from_backend)
            {
                LOG_INFO("removing extent " << eid);
                try
                {
                    bi_->remove(eid.str());
                }
                CATCH_STD_ALL_LOG_IGNORE("Failed to remove " << eid.str() <<
                                         " from the backend - leaking it!");

                extent_exists_(i, false);
                if (size_ % Extent::capacity())
                {
                    size_ -= size_ % Extent::capacity();
                }
                else
                {
                    size_ -= Extent::capacity();
                }
            }
        }
    }
}

void
Container::unlink()
{
    LOCK();

    LOG_INFO(id);
    erase_extents_(true);
}

void
Container::drop_from_cache()
{
    LOCK();

    LOG_TRACE(id);
    erase_extents_(false);
}

void
Container::restart()
{
    LOCK();

    erase_extents_(false);
    extents_.resize(0);
    size_ = 0;

    try
    {
        LOG_TRACE(id);

        std::list<std::string> objs;
        bi_->listObjects(objs);

        for (const auto& o : objs)
        {
            LOG_TRACE("checking " << o);

            try
            {
                ExtentId eid(o);
                if (eid.container_id == id)
                {
                    LOG_INFO("Found extent " << eid);
                    extent_exists_(eid.offset, true);
                }
            }
            catch (...)
            {
                // not an ExtentId - shall we complain instead?
            }
        }

        for (int i = extents_.size() - 1; i >= 0; --i)
        {
            if (extent_exists_(i))
            {
                ExtentId eid(id, i);
                size_ = bi_->getSize(eid.str());
                if (i > 0)
                {
                    size_ += (i - 1) * Extent::capacity();
                }

                break;
            }
        }
    }
    catch (...)
    {
        extents_.resize(0);
        size_ = 0;
        throw;
    }
}

}
