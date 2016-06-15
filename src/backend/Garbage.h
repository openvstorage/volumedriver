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

#ifndef BACKEND_GARBAGE_H_
#define BACKEND_GARBAGE_H_

#include "Namespace.h"

#include <vector>

#include <youtils/Serialization.h>

namespace backend
{

struct Garbage
{
    backend::Namespace nspace;
    using ObjectNames = std::vector<std::string>;
    ObjectNames object_names;

    Garbage(const backend::Namespace& ns,
                   ObjectNames os)
        : nspace(ns)
        , object_names(std::move(os))
    {}

    Garbage() = default;

    ~Garbage() = default;

    Garbage(Garbage&&) = default;

    Garbage&
    operator=(Garbage&&) = default;

    Garbage(const Garbage&) = delete;

    Garbage&
    operator=(const Garbage&) = delete;

    template<typename Archive>
    void
    serialize(Archive& ar,
              const unsigned /* version */)
    {
        ar & BOOST_SERIALIZATION_NVP(nspace);
        ar & BOOST_SERIALIZATION_NVP(object_names);
    }
};

}

BOOST_CLASS_VERSION(backend::Garbage, 1);

#endif // !BACKEND_GARBAGE_H_
