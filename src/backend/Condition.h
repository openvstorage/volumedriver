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

#ifndef BACKEND_CONDITION_H_
#define BACKEND_CONDITION_H_

#include <youtils/Assert.h>
#include <youtils/Logging.h>
#include <youtils/UniqueObjectTag.h>

namespace backend
{

class Condition
{
public:
    Condition(const std::string& o,
              std::unique_ptr<youtils::UniqueObjectTag> t)
        : name_(o)
        , tag_(std::move(t))
    {
        VERIFY(tag_ != nullptr);
    }

    ~Condition() = default;

    Condition(const Condition&) = delete;

    Condition(Condition&&) = default;

    Condition&
    operator=(const Condition&) = delete;

    Condition&
    operator=(Condition&&) = default;

    const youtils::UniqueObjectTag&
    object_tag() const
    {
        VERIFY(tag_ != nullptr);
        return *tag_;
    }

    const std::string&
    object_name() const
    {
        return name_;
    }

private:
    DECLARE_LOGGER("BackendCondition");

    std::string name_;
    std::unique_ptr<youtils::UniqueObjectTag> tag_;
};

}

#endif //!BACKEND_CONDITION_H_
