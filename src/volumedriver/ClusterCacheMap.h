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

#ifndef READCACHE_MAP_H
#define READCACHE_MAP_H

#include "Types.h"

#include <math.h>

#include <list>
#include <numeric>
#include <vector>

#include <boost/intrusive/slist.hpp>

#include <youtils/Assert.h>

namespace volumedriver
{

template<typename K, typename KTraits>
class ClusterCacheMap
{
private:
    using value_type = K;
    using value_type_traits = KTraits;
    using key_type = typename K::key_t;
    using list_type = boost::intrusive::slist<value_type,
                                              value_type_traits,
                                              boost::intrusive::cache_last<true>>;
    using vec_type = std::vector<list_type>;
    using stats_type = std::map<uint64_t, uint64_t>;

    DECLARE_LOGGER("ClusterCacheMap");

public:
    ClusterCacheMap()
        : mask_(0)
        , num_entries_(0)
    {
        VERIFY(max_stats >= 1);
    }

    ~ClusterCacheMap()
    {
        clear();
    }

    void
    resize(uint64_t size)
    {
        VERIFY(size < 64);
        const uint64_t mask = (1UL << size) - 1;
        vec_type vec;
        vec.resize(1UL << size);

        stats_type stats;

        for (auto& l : vec_)
        {
            while (not l.empty())
            {
                auto it = l.begin();
                VERIFY(it != l.end());

                value_type& e = *it;
                l.erase(it);
                insert_(e,
                        vec,
                        stats,
                        mask);
            }
        }

        ASSERT(vec.size() == (1UL << size));
        stats[0] = 1 << size;

        vec_ = std::move(vec);
        stats_ = std::move(stats);

        mask_ = mask;
    }

    void
    resize(uint64_t expected_entries_per_bin,
           uint64_t totalSizeInEntries)
    {
        resize(best_size(expected_entries_per_bin,
                         totalSizeInEntries));
    }

    template<typename F>
    void
    for_each(F&& fun)
    {
        for (auto& slst : vec_)
        {
            for (auto& val : slst)
            {
                fun(val);
            }
        }
    }

    value_type*
    find(const key_type& key)
    {
        uint64_t ind = index_(key);
        list_type& lst = vec_[ind];
        HasKey pred(key);

        auto it = std::find_if(lst.begin(), lst.end(), pred);
        if (it == lst.end())
        {
            return 0;
        }
        else
        {
            return &(*it);
        }
    }

    bool
    spine_empty()
    {
        return vec_.size() == 0;
    }

    void
    insert(value_type& entry)
    {
        insert_(entry,
                vec_,
                stats_,
                mask_);
        ++num_entries_;
    }

    value_type*
    insert_checked(value_type& entry)
    {
        uint64_t ind = index_(entry.key);
        list_type& lst = vec_[ind];
        HasKey pred(entry.key);
        auto it = std::find_if(lst.begin(), lst.end(), pred);
        if (it == lst.end())
        {
            lst.push_back(entry);
            uint64_t size = lst.size();
            VERIFY(size > 0);
            stats_[size - 1]--;
            stats_[size]++;
            ++num_entries_;

            return 0;
        }
        else
        {
            return &(*it);
        }
    }

    bool
    remove(value_type& entry)
    {
        uint64_t ind = index_(entry.key);
        VERIFY(ind < vec_.size());
        list_type& lst = vec_[ind];
        HasKey pred(entry.key);
        auto it = std::find_if(lst.begin(), lst.end(), pred);
        if (it == lst.end())
        {
            return false;
        }
        else
        {
            uint64_t size = lst.size();
            lst.erase(it);
            VERIFY(size > 0);
            stats_[size]--;
            stats_[size - 1]++;
            --num_entries_;
            return true;
        }
    }

    uint64_t
    acum(uint64_t val, std::pair<uint64_t, uint64_t>& p)
    {
        return val + p.second;
    }

    uint64_t
    entries() const
    {
#ifndef NDEBUG
        uint64_t res = 0;

        for (const auto& v : stats_)
        {
            res += v.first*v.second;
        }

        VERIFY(res == num_entries_);
#endif

        return num_entries_;
    }

    bool
    empty() const
    {
        return entries() == 0;
    }

    uint64_t
    spine_size_exp() const
    {
        return ilogb(vec_.size());
    }

    uint64_t
    spine_size() const
    {
        return vec_.size();
    }

    const std::map<uint64_t,uint64_t>&
    stats() const
    {
        return stats_;
    }

    static uint64_t
    best_size(uint64_t expected_entries_per_bin,
              uint64_t read_cache_size_in_clusters)
    {
        if (read_cache_size_in_clusters > expected_entries_per_bin)
        {
            return ilogb((double)read_cache_size_in_clusters / (double)expected_entries_per_bin);
        }
        else
        {
            return 0;
        }
    }

    void
    clear()
    {
        vec_.clear();
        stats_.clear();
        num_entries_ = 0;
    }

private:
    class HasKey
    {
        typedef ClusterCacheMap::key_type key_type;

    public:
        HasKey(const key_type& key)
            : key_(key)
        {}

        bool
        operator()(value_type val)
        {
            return val.key == key_;
        }

    private:
        const key_type& key_;
    };

    uint64_t
    index_(const key_type& key)
    {
        return index_(key,
                      vec_,
                      mask_);
    }

    uint64_t
    index_(const key_type& key,
           const vec_type& vec,
           uint64_t mask)
    {
        uint64_t retval = *(reinterpret_cast<const uint64_t*>(&key)) bitand mask;
        VERIFY(retval < vec.size());
        return retval;
    }

    void
    insert_(value_type& entry,
            vec_type& vec,
            stats_type& stats,
            uint64_t mask)
    {
        uint64_t ind = index_(entry.key,
                              vec,
                              mask);
        list_type& lst = vec[ind];

        lst.push_back(entry);
        uint64_t size = lst.size();
        VERIFY(size > 0);

        stats[size - 1]--;
        stats[size]++;
    }

    static const uint64_t max_stats = 100;

    vec_type vec_;
    uint64_t mask_;

    std::map<uint64_t, uint64_t> stats_;
    uint64_t num_entries_;

    typedef std::map<uint64_t, uint64_t>::value_type stats_val_type;
};

}

#endif // READCACHE_MAP_H

// Local Variables: **
// End: **
