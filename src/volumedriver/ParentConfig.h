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

#ifndef VD_PARENT_CONFIG_H_
#define VD_PARENT_CONFIG_H_

#include <string>

#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/optional.hpp>

#include <youtils/Logging.h>

#include <backend/Namespace.h>

namespace volumedriver
{

struct ParentConfig
{
    backend::Namespace nspace;
    std::string snapshot;

    DECLARE_LOGGER("ParentConfig");

    ParentConfig(const backend::Namespace& ns,
                 const std::string& snap)
        : nspace(ns)
        , snapshot(snap)
    {
        VERIFY(not snapshot.empty());
    }

    ~ParentConfig() = default;

    ParentConfig(const ParentConfig&) = default;

    ParentConfig(ParentConfig& other)
        : nspace(std::move(other.nspace))
        , snapshot(std::move(other.snapshot))
    {}

    ParentConfig&
    operator=(const ParentConfig&) = default;

    ParentConfig&
    operator=(ParentConfig&& other)
    {
        if (this != &other)
        {
            nspace = std::move(other.nspace);
            snapshot = std::move(other.snapshot);
        }

        return *this;
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar,
         const unsigned int /* version */)
    {
        using namespace boost::serialization;

        ar & make_nvp("nspace",
                      nspace);
        ar & make_nvp("snapshot",
                      snapshot);
    }

    template<class Archive>
    void
    save(Archive& ar,
         const unsigned int /* version */) const
    {
        using namespace boost::serialization;

        ar & make_nvp("nspace",
                      nspace);
        ar & make_nvp("snapshot",
                      snapshot);
    }
};

using MaybeParentConfig = boost::optional<ParentConfig>;

}

namespace boost
{
namespace serialization
{

template<typename Archive>
void
load_construct_data(Archive& /* ar */,
                    volumedriver::ParentConfig* cfg,
                    const unsigned /* version */)
{
    new(cfg) volumedriver::ParentConfig(backend::Namespace(std::string("uninitialized")),
                                        "uninitialized");
}

}

}

BOOST_CLASS_VERSION(volumedriver::ParentConfig, 0);

#endif // !VD_PARENT_H_
