// Copyright 2015 iNuron NV
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
