// Copyright (C) 2017 iNuron NV
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

#ifndef BACKEND_PARTIAL_READ_COUNTER_H_
#define BACKEND_PARTIAL_READ_COUNTER_H_

#include <cstdint>

#include <boost/serialization/nvp.hpp>

#include <youtils/Serialization.h>

namespace backend
{

struct PartialReadCounter
{
    uint64_t fast;
    uint64_t slow;

    PartialReadCounter(uint64_t f,
                       uint64_t s)
        : fast(f)
        , slow(s)
    {}

    PartialReadCounter()
        : PartialReadCounter(0, 0)
    {}

    PartialReadCounter(const PartialReadCounter& other) = default;

    PartialReadCounter&
    operator=(const PartialReadCounter&) = default;

    PartialReadCounter&
    operator+=(const PartialReadCounter& other)
    {
        fast += other.fast;
        slow += other.slow;
        return *this;
    }

    friend PartialReadCounter
    operator+(PartialReadCounter lhs,
              const PartialReadCounter& rhs)
    {
        lhs += rhs;
        return lhs;
    }

    bool
    operator==(const PartialReadCounter& other) const
    {
        return
            fast == other.fast and
            slow == other.slow;
    }

    bool
    operator!=(const PartialReadCounter& other) const
    {
        return not operator==(other);
    }

    template<typename Archive>
    void
    serialize(Archive& ar,
              const unsigned /* version */)
    {
        using namespace boost::serialization;

        ar & make_nvp("fast",
                      fast);
        ar & make_nvp("slow",
                      slow);
    }

};

}

BOOST_CLASS_VERSION(backend::PartialReadCounter, 1);

#endif // !BACKEND_PARTIAL_READ_COUNTER_H_
