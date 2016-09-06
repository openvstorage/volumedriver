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

#ifndef YOUTILS_UNIQUE_OBJECT_TAG_H_
#define YOUTILS_UNIQUE_OBJECT_TAG_H_

#include <iosfwd>
#include <string>

namespace youtils
{

class UniqueObjectTag
{
public:
    virtual ~UniqueObjectTag() = default;

    virtual std::string
    str() const = 0;

    bool
    operator==(const UniqueObjectTag& other) const
    {
        return eq(other);
    }

    bool
    operator!=(const UniqueObjectTag& other) const
    {
        return not operator==(other);
    }

protected:
    virtual bool
    eq(const UniqueObjectTag&) const = 0;
};

std::ostream&
operator<<(std::ostream&,
           const UniqueObjectTag&);

}

#endif // !YOUTILS_UNIQUE_OBJECT_TAG_H_
