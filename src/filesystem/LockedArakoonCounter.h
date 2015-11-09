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

#ifndef VFS_LOCKED_ARAKOON_COUNTER_H_
#define VFS_LOCKED_ARAKOON_COUNTER_H_

#include "ClusterId.h"

#include <memory>

#include <youtils/Assert.h>
#include <youtils/LockedArakoon.h>
#include <youtils/Logging.h>

namespace volumedriverfs
{

template<typename>
struct LockedArakoonCounterTraits
{};

template<typename T,
         typename Traits = LockedArakoonCounterTraits<T>>
class LockedArakoonCounter
{
public:
    LockedArakoonCounter(const ClusterId& cluster_id,
                         std::shared_ptr<youtils::LockedArakoon> larakoon)
        : larakoon_(larakoon)
        , cluster_id_(cluster_id)
    {
        maybe_init_();
    }

    ~LockedArakoonCounter() = default;

    LockedArakoonCounter(const LockedArakoonCounter&) = delete;

    LockedArakoonCounter&
    operator=(const LockedArakoonCounter&) = delete;

    T
    operator()()
    {
        T t;

        auto fun([&](arakoon::sequence& seq)
         {
             const std::string key(make_key());
             t = larakoon_->get<std::string, T>(key);
             seq.add_assert(key, t);
             seq.add_set(key,
                         Traits::update_value(t));
         });

        larakoon_->run_sequence("updating counter",
                                std::move(fun),
                                youtils::RetryOnArakoonAssert::T);

        return t;
    }

    void
    destroy()
    {
        larakoon_->delete_prefix(make_key());
    }

    std::string
    make_key() const
    {
        using namespace std::literals::string_literals;
        return cluster_id_.str() + "/"s + Traits::key();
    }

private:
    DECLARE_LOGGER("LockedArakoonCounter");

    std::shared_ptr<youtils::LockedArakoon> larakoon_;
    const ClusterId cluster_id_;

    void
    maybe_init_()
    {
        const std::string key(make_key());

        if (not larakoon_->exists(key))
        {
            auto fun([&](arakoon::sequence& seq)
                     {
                         static const T initval(Traits::initial_value());
                         seq.add_assert(key,
                                        arakoon::None());
                         seq.add_set(key,
                                     initval);
                     });

            try
            {
                larakoon_->run_sequence("initializing counter",
                                        std::move(fun));
            }
            catch (arakoon::error_assertion_failed&)
            {
                LOG_WARN(cluster_id_ <<
                         ": failed to initialize counter " << key <<
                         " - another node beat us to it?");
                THROW_UNLESS(larakoon_->exists(key));
            }
        }
    }

};

}

#endif // !VFS_LOCKED_ARAKOON_COUNTER_H_
