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

#ifndef YT_LOCKED_ARAKOON_H_
#define YT_LOCKED_ARAKOON_H_

#include "ArakoonInterface.h"
#include "Logging.h"

#include <chrono>
#include <functional>

#include <boost/optional.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>

namespace youtils
{

BOOLEAN_ENUM(RetryOnArakoonAssert);

// The idea - besides serializing access to the arakoon client - is to provide a
// number of read-only accessors and to funnel write access through run_sequence.
// This is to support different "views" (e.g. filesystem's ClusterRegistry vs.
// VolumeRegistry at the time of writing) and the composition of arakoon sequences
// from these views, e.g. only allow volume migration (VolumeRegistry's
// responsibility) if the clusternode currently owning it is offline
// (ClusterRegistry's responsibility).
class LockedArakoon
{

#define LOCK()                                  \
    boost::lock_guard<lock_type> lg__(lock_)

public:
    template<typename T>
    LockedArakoon(const arakoon::ClusterID& cluster_id,
                  const T& node_configs,
                  const arakoon::Cluster::MaybeMilliSeconds& mms = boost::none)
        : cluster_id_(cluster_id)
        , arakoon_(connect_(node_configs,
                            mms))
    {}

    virtual ~LockedArakoon() = default;

    LockedArakoon(const LockedArakoon&) = delete;

    LockedArakoon&
    operator=(const LockedArakoon&) = delete;

    template<typename K,
             typename KTraits = arakoon::DataBufferTraits<K> >
    bool
    exists(const K& k)
    {
        LOCK();
        return arakoon_->exists<K, KTraits>(k);
    }

    template<typename K,
             typename V = arakoon::buffer,
             typename KTraits = arakoon::DataBufferTraits<K>,
             typename VTraits = arakoon::DataBufferTraits<V> >
    V
    get(const K& key)
    {
        LOCK();
        return arakoon_->get<K, V, KTraits, VTraits>(key);
    }

    template<typename A,
             typename R,
             typename ATraits = arakoon::DataBufferTraits<A>,
             typename RTraits = arakoon::DataBufferTraits<R> >
    R
    user_function(const A& arg)
    {
        LOCK();
        return arakoon_->user_function<A, R, ATraits, RTraits>(arg);
    }

    template<typename T,
             typename Traits = arakoon::DataBufferTraits<T> >
    arakoon::value_list
    prefix(const T& begin_key,
           const ssize_t max_elements = -1)
    {
        LOCK();
        return arakoon_->prefix<T, Traits>(begin_key, max_elements);
    }

    void
    delete_prefix(const std::string& pfx)
    {
        LOCK();
        arakoon_->delete_prefix(pfx);
    }

    typedef std::function<void(arakoon::sequence&)> PrepareSequenceFun;

    void
    run_sequence(const char* desc,
                 PrepareSequenceFun prep_fun,
                 RetryOnArakoonAssert retry_on_assert = RetryOnArakoonAssert::F);

    // escape hatch / open the flood gates:
    // typedef std::function<void(arakoon::Cluster&)> ArakoonFun;

    // void
    // process(ArakoonFun&& fun)
    // {
    //     LOCK();
    //     fun(arakoon_);
    // }

    // // Templates galore (the arakoon methods are templated too) - so probably
    // // too unwieldy?
    // template<typename R,
    //          typename... Args>
    // R
    // process(R (arakoon::Cluster::*mem_fun),
    //         Args... args)
    // {
    //     LOCK();
    //     return (arakoon_->*mem_fun)(args...);
    // }

    arakoon::ClusterID
    cluster_id() const
    {
        return arakoon_->cluster_id();
    }

    template<typename T>
    void
    reconnect(const T& node_configs,
              const arakoon::Cluster::MaybeMilliSeconds& mms)
    {
        LOG_INFO("Attempting to update arakoon config");

        auto new_one(connect_(node_configs,
                              mms));

        LOCK();
        std::swap(arakoon_,
                  new_one);

        LOG_INFO("New arakoon config in place");
    }

    void
    timeout(const arakoon::Cluster::MaybeMilliSeconds& mms)
    {
        LOG_INFO("setting timeout to " << mms << " ms");

        LOCK();
        arakoon_->timeout(mms);
    }

private:
    DECLARE_LOGGER("LockedArakoon");

    typedef boost::mutex lock_type;
    mutable lock_type lock_;

    const arakoon::ClusterID cluster_id_;
    std::unique_ptr<arakoon::Cluster> arakoon_;

    template<typename T>
    std::unique_ptr<arakoon::Cluster>
    connect_(const T& node_configs,
             const arakoon::Cluster::MaybeMilliSeconds& mms)
    {
        LOG_INFO("Arakoon config: cluster ID " << cluster_id_);
        LOG_INFO("Node configs:");
        for (const auto& c : node_configs)
        {
            LOG_INFO("\t" << c);
        }

        auto arakoon(std::make_unique<arakoon::Cluster>(cluster_id_,
                                                        node_configs,
                                                        mms));

        LOG_INFO("Arakoon configured");
        return arakoon;
    }

#undef LOCK
};

}

#endif // YT_LOCKED_ARAKOON_H_
