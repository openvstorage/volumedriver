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
