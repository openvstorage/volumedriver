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

#ifndef VFS_VOLUME_REGISTRATION_H_
#define VFS_VOLUME_REGISTRATION_H_

#include "NodeId.h"
#include "ObjectTreeConfig.h"
#include "FailOverCacheConfigMode.h"

#include <memory>

#include <boost/make_shared.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/serialization.hpp>

#include <youtils/Assert.h>
#include <youtils/Logging.h>

#include <backend/Namespace.h>

#include <volumedriver/OwnerTag.h>

namespace volumedriverfs
{

class ObjectRegistration
{
public:
    ~ObjectRegistration() = default;

    friend class boost::serialization::access;

    ObjectRegistration(const backend::Namespace& nspc,
                       const ObjectId& volid,
                       const NodeId& node,
                       const ObjectTreeConfig& treeconfig,
                       const volumedriver::OwnerTag tag,
                       const FailOverCacheConfigMode fcm)
        : volume_id(volid)
        , node_id(node)
        , treeconfig(treeconfig)
        , owner_tag(tag)
        , foc_config_mode(fcm)
        , nspace(new backend::Namespace(nspc))
    {}

    ObjectRegistration(const ObjectRegistration& other)
        : volume_id(other.volume_id)
        , node_id(other.node_id)
        , treeconfig(other.treeconfig)
        , owner_tag(other.owner_tag)
        , foc_config_mode(other.foc_config_mode)
        , nspace(new backend::Namespace(other.getNS()))
    {}

    ObjectRegistration&
    operator=(const ObjectRegistration& other) = delete;

    const backend::Namespace&
    getNS() const
    {
        return *nspace;
    }

    // TODO: make this less expensive, avoiding object construction / copying.
    Object
    object() const
    {
        return Object(treeconfig.object_type,
                      volume_id);
    }

    const ObjectId volume_id;
    const NodeId node_id;
    const ObjectTreeConfig treeconfig;
    const volumedriver::OwnerTag owner_tag;
    FailOverCacheConfigMode foc_config_mode;

private:
    DECLARE_LOGGER("ObjectRegistration");

    std::unique_ptr<backend::Namespace> nspace;

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        if (version < 2)
        {
            LOG_FATAL("version " << version << " is not supported");
            THROW_SERIALIZATION_ERROR(version, 2, 2);
        }

        backend::NamespaceDeSerializationHelper ns;
        ar & ns;
        nspace = ns.getNS();

        ar & BOOST_SERIALIZATION_NVP(const_cast<ObjectId&>(volume_id));
        ar & BOOST_SERIALIZATION_NVP(const_cast<NodeId&>(node_id));
        ar & BOOST_SERIALIZATION_NVP(const_cast<ObjectTreeConfig&>(treeconfig));

        if (version > 2)
        {
            ar & BOOST_SERIALIZATION_NVP(const_cast<volumedriver::OwnerTag&>(owner_tag));
        }

        if (version >= 4)
        {
            ar & BOOST_SERIALIZATION_NVP(const_cast<FailOverCacheConfigMode&>(foc_config_mode));
        }
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned int version) const
    {
        if (version != 4)
        {
            THROW_SERIALIZATION_ERROR(version, 4, 4);
        }

        ar & getNS();

        ar & BOOST_SERIALIZATION_NVP(const_cast<ObjectId&>(volume_id));
        ar & BOOST_SERIALIZATION_NVP(const_cast<NodeId&>(node_id));
        ar & BOOST_SERIALIZATION_NVP(const_cast<ObjectTreeConfig&>(treeconfig));
        ar & BOOST_SERIALIZATION_NVP(const_cast<volumedriver::OwnerTag&>(owner_tag));
        ar & BOOST_SERIALIZATION_NVP(const_cast<FailOverCacheConfigMode&>(foc_config_mode));
    }
};

typedef boost::shared_ptr<ObjectRegistration> ObjectRegistrationPtr;

}

namespace boost
{

namespace serialization
{

template<typename Archive>
inline void
load_construct_data(Archive& /* ar */,
                    volumedriverfs::ObjectRegistration* reg,
                    const unsigned /* version */)
{
    using namespace volumedriverfs;

    new(reg) ObjectRegistration(backend::Namespace(),
                                ObjectId("uninitialized"),
                                NodeId("uninitialized"),
                                ObjectTreeConfig::makeBase(),
                                volumedriver::OwnerTag(0),
                                FailOverCacheConfigMode::Automatic);
}

}

}

BOOST_CLASS_VERSION(volumedriverfs::ObjectRegistration, 4);

#endif // VFS_VOLUME_REGISTRATION_H_
