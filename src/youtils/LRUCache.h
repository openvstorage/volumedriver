// Taken from http://timday.bitbucket.org/lru.html and modified to our needs:
// * locking
// * removal of entries
// * adaptation to the fact that retrieval of entries into the cache might be slow
// * only unused elements can be evicted
//
// Copyright notice for the original code:
//
// Copyright (c) 2010-2011, Tim Day <timday@timday.com>

// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.

// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef YOUTILS_LRU_CACHE_H_
#define YOUTILS_LRU_CACHE_H_

#include "Assert.h"
#include "Catchers.h"
#include "IOException.h"
#include "Logging.h"
#include "ScopeExit.h"

#include <atomic>
#include <functional>
#include <future>

#include <boost/bimap.hpp>
#include <boost/bimap/list_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <boost/bimap/set_of.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

namespace youtils
{

template<typename K,
         typename V,
         template<typename...> class Set = boost::bimaps::unordered_set_of>
class LRUCache
{
#define LOCK()                                          \
    boost::lock_guard<decltype(lock_)> llg__(lock_)

public:
    MAKE_EXCEPTION(BusyEntryException, fungi::IOException);
    MAKE_EXCEPTION(BusyCacheException, fungi::IOException);

    typedef K Key;
    typedef V Value;
    typedef std::shared_ptr<Value> ValuePtr;
    typedef std::unique_ptr<Value> UniqueValuePtr;

    typedef std::function<UniqueValuePtr(const K&)> PullFun;
    typedef std::function<void(const K&, Value&)> EvictFun;

    LRUCache(const std::string& name,
             size_t capacity,
             EvictFun&& evict_fun)
        : evict_fun_(std::move(evict_fun))
        , capacity_(capacity)
        , name_(name)
    {
        THROW_WHEN(capacity_ == 0);
        LOG_INFO(name_ << ": capacity " << capacity);
    }

    ~LRUCache() = default;

    LRUCache(const LRUCache&) = delete;

    LRUCache&
    operator=(const LRUCache&) = delete;

    ValuePtr
    find(const K& k,
         PullFun&& fn)
    {
        LOG_TRACE(name_ << ": key " << k);

        boost::unique_lock<decltype(lock_)> u(lock_);

        auto it = bimap_.left.find(k);
        if (it != bimap_.left.end())
        {
            ++hits_;
            bimap_.right.relocate(bimap_.right.end(),
                                  bimap_.project_right(it));
            return it->second;
        }
        else
        {
            ++misses_;
            return fetch_(k, u, std::move(fn));
        }
    }

    bool
    erase(const K& k)
    {
        LOG_TRACE(name_ << ": key " << k);

        LOCK();

        auto it = bimap_.left.find(k);
        if (it != bimap_.left.end())
        {
            const auto& v = it->second;
            if (v.use_count() > 1)
            {
                LOG_ERROR(name_ << ": refusing to remove entry with key " << k <<
                          " as it is still in use");
                throw BusyEntryException("Entry still in use");
            }
            else
            {
                LOG_TRACE(name_ << ": removing " << it->first);
                evict_fun_(it->first, *it->second);
                bimap_.left.erase(it);
                return true;
            }
        }
        else
        {
            return false;
        }
    }

    bool
    erase_no_evict(const K& k)
    {
        LOG_TRACE(name_ << ": key " << k);

        LOCK();

        auto it = bimap_.left.find(k);
        if (it != bimap_.left.end())
        {
            bimap_.left.erase(it);
            return true;
        }
        else
        {
            return false;
        }
    }

    uint64_t
    hits() const
    {
        return hits_;
    }

    uint64_t
    misses() const
    {
        return misses_;
    }

    std::list<K>
    keys() const
    {
        std::list<K> keys;

        LOCK();

        for (auto it = bimap_.right.begin(); it != bimap_.right.end(); ++it)
        {
            keys.push_back(it->second);
        }

        return keys;
    }

    uint32_t
    capacity() const
    {
        LOCK();
        return capacity_;
    }

    void
    capacity(uint32_t cap)
    {
        LOG_INFO(name_ << ": adjusting capacity from " << capacity_ << " to " << cap);
        THROW_WHEN(cap == 0);

        LOCK();

        try
        {
            maybe_evict_(cap);
        }
        CATCH_STD_ALL_LOG_RETHROW(name_ << ": failed to resize the cache from " <<
                                  capacity_ << " to " << cap);

        capacity_ = cap;
    }

    uint32_t
    size()
    {
        LOCK();
        return bimap_.size();
    }
private:
    DECLARE_LOGGER("LRUCache");

    mutable boost::mutex lock_;

    typedef boost::bimaps::bimap<Set<K>,
                                 boost::bimaps::list_of<ValuePtr>> BiMap;

    BiMap bimap_;
    std::atomic<uint64_t> hits_;
    std::atomic<uint64_t> misses_;

    EvictFun evict_fun_;

    uint32_t capacity_;
    const std::string name_;

    typedef std::map<K, std::shared_future<ValuePtr>> Fetchers;
    Fetchers fetchers_;

    ValuePtr
    do_fetch_(const K& k,
              PullFun&& fn)
    {
        // no move here
        ValuePtr v = fn(k);

        LOCK();

        if (v)
        {
            auto it = bimap_.left.find(k);
            if (it == bimap_.left.end())
            {
                try
                {
                    maybe_evict_();
                }
                catch (BusyCacheException& e)
                {
                    LOG_ERROR(name_ << ": failed to add " << k << ": " << e.what());
                    evict_fun_(k, *v);
                    throw;
                }
                ASSERT(bimap_.size() < capacity_);
                bimap_.insert(typename BiMap::value_type(k, v));
            }
        }
        else
        {
            LOG_INFO(name_ << ": key " << k << " does not exist");
        }
        return v;
    }

    ValuePtr
    fetch_(const K& k,
           boost::unique_lock<decltype(lock_)>& u,
           PullFun&& fn)
    {
        LOG_INFO(name_ << ": key " << k);

        while (true)
        {
            auto fit = fetchers_.find(k);
            if (fit == fetchers_.end())
            {
                // no other fetch pending, we're still locked.
                break;
            }

            std::shared_future<ValuePtr> f(fit->second);
            {
                u.unlock();

                auto scope_exit_0 = make_scope_exit([&]()
                                                    {
                                                        u.lock();
                                                        fetchers_.erase(k);
                                                    });

                f.wait();
            }

            ValuePtr v = f.get();

            if (v)
            {
                // we have a hit
                return v;
            }
        }

        // no other thread was fetching it - we have to get our hands dirty ourselves

        // Might be too expensive if our PullFun fn is fast. But then again, if it was
        // fast why put a LRUCache in front of it in the first place?
        std::shared_future<ValuePtr> f = std::async(std::launch::async,
                                                    [&]() -> ValuePtr
                                                    {
                                                        return do_fetch_(k, std::move(fn));
                                                    });
        fetchers_.insert(std::make_pair(k, f));

        u.unlock();

        auto scope_exit_1 = make_scope_exit([&]()
                                            {
                                                u.lock();
                                                fetchers_.erase(k);
                                            });

        f.wait();
        return f.get();
    }

    void
    maybe_evict_(uint32_t cap)
    {
        LOG_TRACE(name_ << ": bimap size " << bimap_.size() << ", target " << cap);

        auto it = bimap_.right.begin();

        while (bimap_.size() > cap)
        {
            if (it == bimap_.right.end())
            {
                LOG_ERROR(name_ <<
                          ": all cache entries are in use, cannot evict one");
                throw BusyCacheException("all cache entries are in use, cannot evict one",
                                         name_.c_str());
            }

            LOG_TRACE(name_ << ": bimap size " << bimap_.size() << ", capacity " <<
                      cap);

            auto& v = it->first;
            if (v.use_count() == 1)
            {
                LOG_TRACE(name_ << ": evicting " << it->second);
                evict_fun_(it->second, *it->first);
                bimap_.right.erase(it);
                it = bimap_.right.begin();
            }
            else
            {
                LOG_TRACE(name_ << ": *not* evicting " << it->second <<
                          " as it's in use (" << v.use_count() << ")");
                ++it;
            }
        }
    }

    void
    maybe_evict_()
    {
        maybe_evict_(capacity_ - 1);
    }

#undef LOCK
};

}

#endif // !YOUTILS_LRU_CACHE_H_
