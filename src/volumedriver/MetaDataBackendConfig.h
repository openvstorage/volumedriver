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

#ifndef VD_META_DATA_BACKEND_CONFIG_H_
#define VD_META_DATA_BACKEND_CONFIG_H_

#include "MDSNodeConfig.h"

#include <chrono>
#include <iosfwd>
#include <type_traits>
#include <vector>

#include <boost/serialization/export.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>

#include <youtils/ArakoonNodeConfig.h>
#include <youtils/Serialization.h>

namespace volumedriver
{

enum class MetaDataBackendType
{
    Arakoon,
    MDS,
    RocksDB,
    TCBT,
};

std::ostream&
operator<<(std::ostream& os,
           MetaDataBackendType t);

std::istream&
operator>>(std::istream& is,
           MetaDataBackendType& t);

struct MetaDataBackendConfig
{
    explicit MetaDataBackendConfig(MetaDataBackendType t)
        : backend_type_(t)
    {}

    virtual ~MetaDataBackendConfig() = default;

    MetaDataBackendType
    backend_type() const
    {
        return backend_type_;
    }

    virtual std::unique_ptr<MetaDataBackendConfig>
    clone() const = 0;

    bool
    operator==(const MetaDataBackendConfig& other) const
    {
        return backend_type_ == other.backend_type_ and equals(other);
    }

protected:
    virtual bool
    equals(const MetaDataBackendConfig&) const = 0;

    MetaDataBackendType backend_type_;

private:
    friend class boost::serialization::access;

    // only used for deserialization
    MetaDataBackendConfig()
        : backend_type_(MetaDataBackendType::TCBT)
    {}

    template<typename A>
    void
    serialize(A& ar,
              const unsigned version)
    {
        if (version != 1)
        {
            // No backward compatibility for now.
            // The below checks are left in place in case we ever want to change that
            // and serve as documentation.
            THROW_SERIALIZATION_ERROR(version, 1, 1);
        }

        ar & BOOST_SERIALIZATION_NVP(backend_type_);
    }
};

// NOTE: these are supposed to be final, but that breaks boost::serialization as
// it plays games with type traits.
struct TCBTMetaDataBackendConfig
    : public MetaDataBackendConfig
{
    TCBTMetaDataBackendConfig()
        : MetaDataBackendConfig(MetaDataBackendType::TCBT)
    {}

    virtual ~TCBTMetaDataBackendConfig() = default;

    virtual std::unique_ptr<MetaDataBackendConfig>
    clone() const override final
    {
        return std::unique_ptr<MetaDataBackendConfig>(new TCBTMetaDataBackendConfig());
    }

protected:
    virtual bool
    equals(const MetaDataBackendConfig& other) const override final
    {
        return dynamic_cast<const TCBTMetaDataBackendConfig*>(&other) != nullptr;
    }

private:
    friend class boost::serialization::access;

    template<typename A>
    void
    serialize(A& /* ar */,
              const unsigned version)
    {
        if (version != 1)
        {
            // No backward compatibility for now.
            // The below checks are left in place in case we ever want to change that
            // and serve as documentation.
            THROW_SERIALIZATION_ERROR(version, 1, 1);
        }

        boost::serialization::void_cast_register<TCBTMetaDataBackendConfig,
                                                 MetaDataBackendConfig>();
    }
};

struct RocksDBMetaDataBackendConfig
    : public MetaDataBackendConfig
{
    RocksDBMetaDataBackendConfig()
        : MetaDataBackendConfig(MetaDataBackendType::RocksDB)
    {}

    virtual ~RocksDBMetaDataBackendConfig() = default;

    virtual std::unique_ptr<MetaDataBackendConfig>
    clone() const override final
    {
        return std::unique_ptr<MetaDataBackendConfig>(new RocksDBMetaDataBackendConfig());
    }

protected:
    virtual bool
    equals(const MetaDataBackendConfig& other) const override final
    {
        return dynamic_cast<const RocksDBMetaDataBackendConfig*>(&other) != nullptr;
    }

private:
    friend class boost::serialization::access;

    template<typename A>
    void
    serialize(A& /* ar */,
              const unsigned version)
    {
        if (version != 1)
        {
            // No backward compatibility for now.
            // The below checks are left in place in case we ever want to change that
            // and serve as documentation.
            THROW_SERIALIZATION_ERROR(version, 1, 1);
        }

        boost::serialization::void_cast_register<RocksDBMetaDataBackendConfig,
                                                 MetaDataBackendConfig>();
    }
};

struct ArakoonMetaDataBackendConfig
    : public MetaDataBackendConfig
{
    template<typename T>
    ArakoonMetaDataBackendConfig(const arakoon::ClusterID& cid,
                          const T& cfgs)
        : MetaDataBackendConfig(MetaDataBackendType::Arakoon)
        , cluster_id_(cid)
        , node_configs_(cfgs.begin(),
                       cfgs.end())
    {
        if (node_configs_.empty())
        {
            LOG_ERROR("Empty node configs are not permitted");
            throw fungi::IOException("Empty node configs are not permitted");
        }
    }

    virtual ~ArakoonMetaDataBackendConfig() = default;

    const arakoon::ClusterID&
    cluster_id() const
    {
        return cluster_id_;
    }

    typedef std::vector<arakoon::ArakoonNodeConfig> NodeConfigs;

    const NodeConfigs&
    node_configs() const
    {
        return node_configs_;
    }

    virtual std::unique_ptr<MetaDataBackendConfig>
    clone() const override final
    {
        return std::unique_ptr<MetaDataBackendConfig>(new ArakoonMetaDataBackendConfig(cluster_id(),
                                                                                       node_configs()));
    }

protected:
    virtual bool
    equals(const MetaDataBackendConfig& other) const override final
    {
        auto o(dynamic_cast<const ArakoonMetaDataBackendConfig*>(&other));
        return o != nullptr and
            cluster_id_ == o->cluster_id_ and
            node_configs_ == o->node_configs_;
    }

private:
    DECLARE_LOGGER("ArakoonMetaDataBackendConfig");

    arakoon::ClusterID cluster_id_;
    NodeConfigs node_configs_;

    friend class boost::serialization::access;

    // only used for deserialization
    ArakoonMetaDataBackendConfig()
        : MetaDataBackendConfig(MetaDataBackendType::Arakoon)
    {}

    template<typename A>
    void
    serialize(A& ar,
              const unsigned version)
    {
        if (version != 1)
        {
            // No backward compatibility for now.
            // The below checks are left in place in case we ever want to change that
            // and serve as documentation.
            THROW_SERIALIZATION_ERROR(version, 1, 1);
        }

        boost::serialization::void_cast_register<ArakoonMetaDataBackendConfig,
                                                 MetaDataBackendConfig>();

        ar & BOOST_SERIALIZATION_NVP(cluster_id_);
        ar & BOOST_SERIALIZATION_NVP(node_configs_);
    }
};

VD_BOOLEAN_ENUM(ApplyRelocationsToSlaves);

struct MDSMetaDataBackendConfig
    : public MetaDataBackendConfig
{
    static unsigned default_timeout_secs_;

    template<typename T>
    MDSMetaDataBackendConfig(const T& cfgs,
                             ApplyRelocationsToSlaves apply_scrub,
                             unsigned timeout_secs = default_timeout_secs_,
                             const boost::optional<uint32_t>& max_tlogs_behind = boost::none)
        : MetaDataBackendConfig(MetaDataBackendType::MDS)
        , node_configs_(cfgs.begin(),
                        cfgs.end())
        , apply_relocations_to_slaves_(apply_scrub)
        , timeout_(timeout_secs)
        , max_tlogs_behind_(max_tlogs_behind)
    {
        if (timeout_secs == 0)
        {
            LOG_ERROR("MDS timeout of 0 secs is not permitted");
            throw fungi::IOException("MDS timeout of 0 seconds is not permitted");
        }

        if (node_configs_.empty())
        {
            LOG_ERROR("Empty node configs are not permitted");
            throw fungi::IOException("Empty node configs are not permitted");
        }
    }

    virtual ~MDSMetaDataBackendConfig() = default;

    typedef std::vector<MDSNodeConfig> NodeConfigs;

    const NodeConfigs&
    node_configs() const
    {
        return node_configs_;
    }

    ApplyRelocationsToSlaves
    apply_relocations_to_slaves() const
    {
        return apply_relocations_to_slaves_;
    }

    std::chrono::seconds
    timeout() const
    {
        return timeout_;
    }

    boost::optional<uint32_t>
    max_tlogs_behind() const
    {
        return max_tlogs_behind_;
    }

    virtual std::unique_ptr<MetaDataBackendConfig>
    clone() const override final
    {
        return std::unique_ptr<MetaDataBackendConfig>(new MDSMetaDataBackendConfig(node_configs(),
                                                                                   apply_relocations_to_slaves(),
                                                                                   timeout_.count(),
                                                                                   max_tlogs_behind()));
    }

protected:
    virtual bool
    equals(const MetaDataBackendConfig& other) const override final
    {
        auto o(dynamic_cast<const MDSMetaDataBackendConfig*>(&other));
        return o != nullptr and
            node_configs_ == o->node_configs_ and
            apply_relocations_to_slaves_ == o->apply_relocations_to_slaves_ and
            timeout_ == o->timeout_ and
            max_tlogs_behind_ == o->max_tlogs_behind_;
    }

private:
    DECLARE_LOGGER("MDSMetaDataBackendConfig");

    NodeConfigs node_configs_;
    ApplyRelocationsToSlaves apply_relocations_to_slaves_;
    std::chrono::seconds timeout_;
    boost::optional<uint32_t> max_tlogs_behind_;

    friend class boost::serialization::access;

    // only used for deserialization
    MDSMetaDataBackendConfig()
        : MetaDataBackendConfig(MetaDataBackendType::MDS)
        , apply_relocations_to_slaves_(ApplyRelocationsToSlaves::T)
    {}

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<typename A>
    void
    load(A& ar,
         const unsigned version)
    {
        boost::serialization::void_cast_register<MDSMetaDataBackendConfig,
                                                 MetaDataBackendConfig>();

        ar & BOOST_SERIALIZATION_NVP(node_configs_);

        if (version > 1)
        {
            ar & BOOST_SERIALIZATION_NVP(apply_relocations_to_slaves_);
        }
        else
        {
            apply_relocations_to_slaves_ = ApplyRelocationsToSlaves::T;
        }

        if (version > 2)
        {
            uint64_t t;
            ar & boost::serialization::make_nvp("timeout_secs",
                                                t);
            timeout_ = std::chrono::seconds(t);
        }
        else
        {
            timeout_ = std::chrono::seconds(default_timeout_secs_);
        }

        if (version > 3)
        {
            ar & BOOST_SERIALIZATION_NVP(max_tlogs_behind_);
        }
        else
        {
            max_tlogs_behind_ = boost::none;
        }
    }

    template<typename A>
    void
    save(A& ar,
         const unsigned version) const
    {
        CHECK_VERSION(version, 4);

        boost::serialization::void_cast_register<MDSMetaDataBackendConfig,
                                                 MetaDataBackendConfig>();

        ar & BOOST_SERIALIZATION_NVP(node_configs_);
        ar & BOOST_SERIALIZATION_NVP(apply_relocations_to_slaves_);
        uint64_t t = timeout_.count();
        ar & boost::serialization::make_nvp("timeout_secs",
                                            t);
        ar & BOOST_SERIALIZATION_NVP(max_tlogs_behind_);
    }
};

}

BOOST_CLASS_VERSION(volumedriver::MetaDataBackendConfig, 1);
BOOST_CLASS_EXPORT_KEY(volumedriver::MetaDataBackendConfig);

BOOST_CLASS_VERSION(volumedriver::TCBTMetaDataBackendConfig, 1);
BOOST_CLASS_EXPORT_KEY(volumedriver::TCBTMetaDataBackendConfig);

BOOST_CLASS_VERSION(volumedriver::RocksDBMetaDataBackendConfig, 1);
BOOST_CLASS_EXPORT_KEY(volumedriver::RocksDBMetaDataBackendConfig);

BOOST_CLASS_VERSION(volumedriver::ArakoonMetaDataBackendConfig, 1);
BOOST_CLASS_EXPORT_KEY(volumedriver::ArakoonMetaDataBackendConfig);

BOOST_CLASS_VERSION(volumedriver::MDSMetaDataBackendConfig, 4);
BOOST_CLASS_EXPORT_KEY(volumedriver::MDSMetaDataBackendConfig);

#endif // !VD_META_DATA_BACKEND_CONFIG_H_
