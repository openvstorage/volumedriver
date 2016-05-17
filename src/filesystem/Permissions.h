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

#ifndef VFS_PERMISSIONS_H_
#define VFS_PERMISSIONS_H_

#include <sys/stat.h>

#include <iomanip>
#include <iosfwd>

#include <boost/io/ios_state.hpp>

#include <youtils/Serialization.h>

namespace volumedriverfs
{

struct Permissions
{
    explicit Permissions(mode_t m)
        : mode(m bitand ~S_IFMT)
    {}

    ~Permissions() = default;

    Permissions(const Permissions&) = default;

    Permissions&
    operator=(const Permissions& other)
    {
        if (this != &other)
        {
            const_cast<mode_t&>(mode) = other.mode;
        }

        return *this;
    }

    operator mode_t() const
    {
        return mode;
    }

    bool
    operator==(const Permissions& other) const
    {
        return mode == other.mode;
    }

    bool
    operator!=(const Permissions& other) const
    {
        return mode != other.mode;
    }

    bool
    operator<(const Permissions& other) const
    {
        return mode < other.mode;
    }

    bool
    operator>(const Permissions& other) const
    {
        return mode > other.mode;
    }

    template<typename A>
    void
    serialize(A& ar, const unsigned version)
    {
        CHECK_VERSION(version, 1);

        ar & boost::serialization::make_nvp("mode",
                                            const_cast<mode_t&>(mode));
    }

    const mode_t mode;
};

std::ostream&
operator<<(std::ostream& os,
           const Permissions pm);

}

BOOST_CLASS_VERSION(volumedriverfs::Permissions, 1);

#endif // !VFS_PERMISSIONS_H_
