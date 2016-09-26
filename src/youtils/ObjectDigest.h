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

#ifndef YOUTILS_OBJECT_DIGEST_H_
#define YOUTILS_OBJECT_DIGEST_H_

#include "Md5.h"
#include "Sha1.h"
#include "UniqueObjectTag.h"

#include <boost/lexical_cast.hpp>

namespace youtils
{

template<typename MessageDigest>
class ObjectDigest
    : public UniqueObjectTag
{
public:
    explicit ObjectDigest(const MessageDigest& w)
        : digest_(w)
    {}

    virtual ~ObjectDigest() = default;

    std::string
    str() const override final
    {
        return boost::lexical_cast<std::string>(digest_);
    }

    bool
    eq(const UniqueObjectTag& other) const override final
    {
        auto digest = dynamic_cast<const ObjectDigest*>(&other);
        return digest ? operator==(*digest) : false;
    }

    bool
    operator==(const ObjectDigest& other) const
    {
        return digest_ == other.digest_;
    }

    bool
    operator!=(const ObjectDigest& other) const
    {
        return digest_ != other.digest_;
    }

private:
    MessageDigest digest_;
};

using ObjectMd5 = ObjectDigest<Md5>;
using ObjectSha1 = ObjectDigest<Sha1>;

}

#endif // !YOUTILS_OBJECT_DIGEST_H_
