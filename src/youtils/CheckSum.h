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

#ifndef CHECKSUM_H_
#define CHECKSUM_H_

#include "Logging.h"

#include <stdint.h>
#include <exception>
#include <iosfwd>

#include <boost/crc.hpp>

namespace youtilstest
{

class CheckSumTest;

}

namespace youtils
{

class CheckSumException
    : public std::exception
{
public:
    virtual const char*
    what() const throw ()
    {
        return "CheckSum exception";
    }
};

class CheckSum
{
    friend class youtilstest::CheckSumTest;

public:
    using value_type = uint32_t;

    explicit CheckSum(value_type crc)
        : crc_(~crc)
    {}

    CheckSum()
        : crc_(initial_value_)
    {}

    ~CheckSum() = default;

    CheckSum(const CheckSum&) = default;

    CheckSum&
    operator=(const CheckSum&) = default;

    bool
    operator==(const CheckSum& other) const
    {
        return crc_ == other.crc_;
    }

    bool
    operator!=(const CheckSum& other) const
    {
        return not (*this == other);
    }

    void
    update(const void* buf, uint64_t bufsize);

    value_type
    getValue() const
    {
        return ~crc_;
    }

    void
    reset()
    {
        crc_ = initial_value_;
    }

private:
    DECLARE_LOGGER("CheckSum");

    static constexpr value_type initial_value_ = ~0U;

    value_type crc_;

    static value_type
    crc32c_sw_(value_type crc,
               const void* data,
               size_t length);

    static value_type
    crc32c_hw_(value_type crc,
               const void* data,
               size_t length);
};

std::ostream&
operator<<(std::ostream& os, const CheckSum& cs);

}

#endif // !CHECKSUM_H_

// Local Variables: **
// mode: c++ **
// End: **
