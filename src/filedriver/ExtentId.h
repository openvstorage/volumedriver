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

#ifndef FILE_DRIVER_EXTENT_ID_H_
#define FILE_DRIVER_EXTENT_ID_H_

#include "Container.h"

#include <string>

#include <boost/lexical_cast.hpp>

#include <youtils/IOException.h>
#include <youtils/Logging.h>

namespace filedrivertest
{
class ExtentIdTest;
}

namespace filedriver
{

class ExtentId
{
    friend class filedrivertest::ExtentIdTest;

public:
    MAKE_EXCEPTION(NotAnExtentId, fungi::IOException);

    ExtentId(const ContainerId& cid,
             uint32_t off)
        : container_id(cid)
        , offset(off)
    {}

    explicit ExtentId(const std::string& str);

    ~ExtentId() = default;

    ExtentId(const ExtentId&) = default;

    ExtentId(ExtentId&& other)
        : container_id(std::move(other.container_id))
        , offset(std::move(other.offset))
    {}

    ExtentId&
    operator=(const ExtentId& other)
    {
        if (this != &other)
        {
            const_cast<ContainerId&>(container_id) = other.container_id;
            const_cast<uint32_t&>(offset) = other.offset;
        }

        return *this;
    }

    bool
    operator==(const ExtentId& other) const
    {
        return container_id == other.container_id and
            offset == other.offset;
    }

    bool
    operator!=(const ExtentId& other) const
    {
        return not operator==(other);
    }

    bool
    operator<(const ExtentId& other) const
    {
        if (container_id < other.container_id)
        {
            return true;
        }
        else if (container_id == other.container_id)
        {
            return offset < other.offset;
        }
        else
        {
            return false;
        }
    }

    std::string
    str() const
    {
        return boost::lexical_cast<std::string>(*this);
    }

    friend std::ostream&
    operator<<(std::ostream& os,
               const ExtentId& eid)
    {
        boost::io::ios_all_saver osg(os);
        return os << eid.container_id << ExtentId::separator_ <<
            std::hex << std::setfill('0') << std::setw(suffix_size_) <<
            eid.offset;
    }

    const ContainerId container_id;
    const uint32_t offset;

private:
    DECLARE_LOGGER("FileDriverExtentId");

    static const std::string separator_;
    static const size_t suffix_size_;
};

}

#endif // !FILE_DRIVER_EXTENT_ID_H_
