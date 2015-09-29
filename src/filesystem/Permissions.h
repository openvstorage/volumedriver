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
