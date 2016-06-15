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

#include "UUID.h"
#include "IOException.h"
#include "Assert.h"

namespace youtils
{

namespace bu = boost::uuids;

fungi::SpinLock UUID::random_generator_lock_;
bu::random_generator UUID::random_generator_;

static_assert(sizeof(bu::uuid) == 16,
              "unexpected bu::uuid size");

static_assert(sizeof(UUID) == 16,
              "unexpected UUID size");

UUID::UUID()
{
    fungi::ScopedSpinLock sl(random_generator_lock_);
    uuid_ = random_generator_();
}

UUID::UUID(const UUID& other)
{
    uuid_ = other.uuid_;
}

inline bool
UUID::hasonlyhexdigits(const std::string& str,
                 size_t i1,
                 size_t i2)
{
    VERIFY(i1 <= i2);
    for(size_t i = i1; i < i2; ++i)
    {
        if(not std::isxdigit(str[i]))
        {
            return false;
        }
    }
    return true;
}

bool
UUID::isUUIDString(const std::string& in)
{
    return in.size() == 36 and
        hasonlyhexdigits(in, 0,8) and
        in[8] == '-' and
        hasonlyhexdigits(in,9,13) and
        in[13] == '-' and
        hasonlyhexdigits(in,14,18) and
        in[18] == '-' and
        hasonlyhexdigits(in,19,23) and
        in[23] == '-' and
        hasonlyhexdigits(in,24,36);
}

inline bool
UUID::hasonlyhexdigits(const char* in,
                       size_t i1,
                       size_t i2)
{
    VERIFY(i1 <= i2);
    for(size_t i = i1; i < i2; ++i)
    {
        if(not std::isxdigit(in[i]))
        {
            return false;
        }
    }
    return true;
}

bool
UUID::isUUIDString(const char* in)
{
    return
        hasonlyhexdigits(in, 0,8) and
        in[8] == '-' and
        hasonlyhexdigits(in,9,13) and
        in[13] == '-' and
        hasonlyhexdigits(in,14,18) and
        in[18] == '-' and
        hasonlyhexdigits(in,19,23) and
        in[23] == '-' and
        hasonlyhexdigits(in,24,36);
}

UUID::UUID(const std::string& in)
{
    if(isUUIDString(in))
    {
        std::stringstream ss(in);
        ss >> uuid_;
    }
    else
    {
        LOG_ERROR("Failed to parse uuid " << in);
        throw fungi::IOException("Failed to parse uuid",
                                 in.c_str());
    }
}

UUID::UUID(const char* in)
{
    if(isUUIDString(in))
    {
        const std::string str(in, getUUIDStringSize());
        std::stringstream ss(str);
        ss >> uuid_;
    }
        else
    {
        LOG_ERROR("Failed to parse uuid " << std::string(in, getUUIDStringSize()));
        throw fungi::IOException("Failed to parse uuid");
    }
}

bool
UUID::operator==(const UUID& other) const
{
    return (other.uuid_ == uuid_);
}

UUID&
UUID::operator=(const UUID& other)
{
    if (this != &other)
    {
        uuid_ = other.uuid_;
    }
    return *this;
}

bool
UUID::operator!=(const UUID& other) const
{
    return not (*this == other);
}

bool
UUID::operator<(const UUID& other) const
{
    return uuid_ < other.uuid_;
}

bool
UUID::operator>(const UUID& other) const
{
    return uuid_ > other.uuid_;
}

std::string
UUID::str() const
{
   return to_string(uuid_);
}

bool
UUID::isNull() const
{
    return uuid_.is_nil();
}

std::ostream&
operator<<(std::ostream& ostr, const UUID& uuid)
{
    return (ostr << uuid.uuid_);
}

}

// Local Variables: **
// mode: c++ **
// End: **
