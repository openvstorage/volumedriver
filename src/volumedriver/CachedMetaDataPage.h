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

#ifndef CACHED_META_DATA_PAGE_H_
#define CACHED_META_DATA_PAGE_H_

#include "ClusterLocationAndHash.h"
#include "Types.h"

#include <boost/thread/mutex.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/intrusive/list.hpp>

namespace volumedriver
{
namespace bi = boost::intrusive;

typedef bi::list_base_hook<bi::link_mode<bi::auto_unlink> > list_base_hook;
typedef bi::set_base_hook<bi::link_mode<bi::auto_unlink> > set_base_hook;

// Uses malloc/free for the data_ member as it interfaces with C libs
// (tokyocabinet, crakoon)
class CachePage
    : public list_base_hook
    , public set_base_hook
{
public:
    explicit CachePage(const PageAddress& pa,
                       ClusterLocationAndHash* data)
        : page_address_(pa)
        , data_(data)
        , dirty(false)
        , written_clusters_since_last_backend_write(0)
        , discarded_clusters_since_last_backend_write(0)
    {
        ASSERT(data_ != nullptr);
    }

    CachePage(const CachePage& other) = delete;

    CachePage(const CachePage& other,
              ClusterLocationAndHash* data)
        : page_address_(other.page_address_)
        , data_(data)
        , dirty(other.dirty)
        , written_clusters_since_last_backend_write(other.written_clusters_since_last_backend_write)
        , discarded_clusters_since_last_backend_write(other.discarded_clusters_since_last_backend_write)
    {
        memcpy(data_, other.data_, size());
    }

    // std::vector<CachePage>::reserve() needs a copy or a move constructor
    CachePage(const CachePage&& other)
        : page_address_(other.page_address_)
        , data_(other.data_)
        , dirty(other.dirty)
        , written_clusters_since_last_backend_write(other.written_clusters_since_last_backend_write)
        , discarded_clusters_since_last_backend_write(other.discarded_clusters_since_last_backend_write)
    {}

    CachePage&
    operator=(const CachePage& other)
    {
        if (this != &other)
        {
            ASSERT(data_ != nullptr);
            ASSERT(other.data_ != nullptr);

            const_cast<PageAddress&>(page_address_) = other.page_address_;
            dirty = other.dirty;
            written_clusters_since_last_backend_write =
                other.written_clusters_since_last_backend_write;
            discarded_clusters_since_last_backend_write =
                other.discarded_clusters_since_last_backend_write;

            memcpy(data_, other.data_, size());
        }

        return *this;
    }

    ~CachePage() = default;

    inline void
    unlink_from_list()
    {
        list_base_hook::unlink();
    }

    inline void
    unlink_from_set()
    {
        set_base_hook::unlink();
    }

    inline bool
    is_in_list() const
    {
        return list_base_hook::is_linked();
    }

    inline bool
    is_in_set() const
    {
        return set_base_hook::is_linked();
    }

    bool
    operator>(const CachePage& rhs) const
    {
        return page_address_ > rhs.page_address_;
    }

    bool
    operator<(const CachePage& rhs) const
    {
        return page_address_ < rhs.page_address_;
    }

    bool
    operator==(const CachePage& rhs) const
    {
        return page_address_ == rhs.page_address_;
    }

    static inline uint32_t
    capacity()
    {
        return 1U << page_bits_;
    }

    inline const PageAddress&
    page_address() const
    {
        return page_address_;
    }

    static inline uint32_t
    offset(ClusterAddress ca)
    {
        return (ca & (capacity() - 1));
    }

    inline bool
    empty() const
    {
        for (size_t i = 0; i < capacity(); ++i)
        {
            if (not data_[i].clusterLocation.isNull())
            {
                return false;
            }
        }

        return true;
    }

    void
    incrementPageCloneID(const SCOCloneID increase = SCOCloneID(1),
                         bool set_dirty = true)
    {
        for(size_t i = 0; i < capacity(); ++i)
        {
            ClusterLocation& cl = data_[i].clusterLocation;
            if(not cl.isNull())
            {
                cl.incrementCloneID(increase);
            }
        }
        dirty = set_dirty;
    }

    inline ClusterLocationAndHash&
    operator[](size_t i)
    {
        // ASSERT HERE!!!
        ASSERT(i < capacity());
        return data_[i];
    }

    inline const ClusterLocationAndHash&
    operator[](size_t i) const
    {
        // ASSERT HERE!!!
        ASSERT(i < capacity());
        return data_[i];
    }

    void
    reset()
    {
        memset(data_, 0x0, size());
    }

    const ClusterLocationAndHash*
    data() const
    {
        return data_;
    }

    ClusterLocationAndHash*
    data()
    {
        return data_;
    }

    static size_t
    size()
    {
        return capacity() * sizeof(ClusterLocationAndHash);
    }

    static inline PageAddress
    pageAddress(ClusterAddress ca)
    {
        return PageAddress(ca >> page_bits_);
    }

    static inline ClusterAddress
    clusterAddress(PageAddress pa)
    {
        return pa << page_bits_;
    }

private:
    DECLARE_LOGGER("CachedMetaDataPage");

    const static uint8_t page_bits_;
    PageAddress page_address_;
    ClusterLocationAndHash* data_;

public:
    bool dirty;
    uint32_t written_clusters_since_last_backend_write;
    uint32_t discarded_clusters_since_last_backend_write;
};

}

#endif // CACHED_META_DATA_PAGE_H_

// Local Variables: **
// mode: c++ **
// End: **
