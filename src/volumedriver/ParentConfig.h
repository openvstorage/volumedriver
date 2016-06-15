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

#ifndef VD_PARENT_CONFIG_H_
#define VD_PARENT_CONFIG_H_

#include "SnapshotName.h"

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
    SnapshotName snapshot;

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

        std::string snap;
        ar & make_nvp("snapshot",
                      snap);

        snapshot = SnapshotName(snap);
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
                      static_cast<const std::string>(snapshot));
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
                                        volumedriver::SnapshotName("uninitialized"));
}

}

}

BOOST_CLASS_VERSION(volumedriver::ParentConfig, 0);

#endif // !VD_PARENT_H_
