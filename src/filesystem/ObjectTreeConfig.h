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

#ifndef VFS_OBJECT_TREE_CONFIG_H_
#define VFS_OBJECT_TREE_CONFIG_H_

#include "Object.h"

#include <map>

#include <boost/optional.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/serialization.hpp>

#include <youtils/Assert.h>

namespace volumedriverfs
{

class ObjectRegistration;

class ObjectTreeConfig
{
    friend class boost::serialization::access;
    friend class ObjectRegistration;

public:
    typedef std::map<ObjectId, MaybeSnapshotName> Descendants;

    const ObjectType object_type;
    const boost::optional<ObjectId> parent_volume;
    const Descendants descendants;

    static ObjectTreeConfig
    makeBase()
    {
        return ObjectTreeConfig(ObjectType::Volume,
                                Descendants(),
                                boost::none);
    }

    static ObjectTreeConfig
    makeClone(const ObjectId& parent)
    {
        return ObjectTreeConfig(ObjectType::Volume,
                                Descendants(),
                                parent);
    }

    static ObjectTreeConfig
    makeTemplate(const boost::optional<ObjectId>& parent)
    {
        return ObjectTreeConfig(ObjectType::Template,
                                Descendants(),
                                parent);
    }

    static ObjectTreeConfig
    makeParent(ObjectType type,
               const Descendants& descendants,
               const boost::optional<ObjectId>& grandparent)
    {
        return ObjectTreeConfig(type,
                                descendants,
                                grandparent);
    }

    static ObjectTreeConfig
    makeFile()
    {
        return ObjectTreeConfig(ObjectType::File,
                                Descendants(),
                                boost::none);
    }

private:
    ObjectTreeConfig(ObjectType type,
                     const Descendants& offspring,
                     const boost::optional<ObjectId>& parent)
        : object_type(type)
        , parent_volume(parent)
        , descendants(offspring)
    {}

    template<class Archive>
    void
    serialize(Archive& ar, const unsigned int version)
    {
        CHECK_VERSION(version, 2);

        ar & BOOST_SERIALIZATION_NVP(const_cast<ObjectType&>(object_type));
        ar & BOOST_SERIALIZATION_NVP(const_cast<boost::optional<ObjectId>& >(parent_volume));
        ar & BOOST_SERIALIZATION_NVP(const_cast<Descendants&>(descendants));
    }
};

}

namespace boost
{

namespace serialization
{

template<typename Archive>
inline void
load_construct_data(Archive& /* ar */,
                    volumedriverfs::ObjectTreeConfig* cfg,
                    const unsigned /* version */)
{
    using namespace volumedriverfs;

    new(cfg) ObjectTreeConfig(ObjectTreeConfig::makeBase());
}

}

}

BOOST_CLASS_VERSION(volumedriverfs::ObjectTreeConfig, 2);

#endif // !VFS_OBJECT_TREE_CONFIG_H_
