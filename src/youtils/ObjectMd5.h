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

#ifndef YOUTILS_OBJECT_MD5_H_
#define YOUTILS_OBJECT_MD5_H_

#include "UniqueObjectTag.h"
#include "Weed.h"

#include <boost/lexical_cast.hpp>

namespace youtils
{

class ObjectMd5
    : public UniqueObjectTag
{
public:
    explicit ObjectMd5(const Weed& w)
        : weed_(w)
    {}

    virtual ~ObjectMd5() = default;

    std::string
    str() const override final
    {
        return boost::lexical_cast<std::string>(weed_);
    }

    bool
    eq(const UniqueObjectTag& other) const override final
    {
        auto md5 = dynamic_cast<const ObjectMd5*>(&other);
        return md5 ? operator==(*md5) : false;
    }

    bool
    operator==(const ObjectMd5& other) const
    {
        return weed_ == other.weed_;
    }

    bool
    operator!=(const ObjectMd5& other) const
    {
        return weed_ != other.weed_;
    }

private:
    Weed weed_;
};

}

#endif // !YOUTILS_OBJECT_MD5_H_
