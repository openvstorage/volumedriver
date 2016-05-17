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

#ifndef VD_VOLUME_CONFIG_PARAMETERS_H_
#define VD_VOLUME_CONFIG_PARAMETERS_H_

#include "MetaDataBackendConfig.h"
#include "OwnerTag.h"
#include "SnapshotName.h"
#include "Types.h"
#include "VolumeConfig.h"

#include <boost/optional.hpp>

#include <youtils/ArakoonNodeConfig.h>

#include <backend/Namespace.h>

namespace volumedriver
{

// The idea is to use the "named parameter" idiom - required params need to be specified
// in the constructor whereas the optional ones can be set with setters.
//
// Runtime polymorphism doesn't help us here, as setters like these
// ... Base& Base::set_foo() ...
// ... Derived& Derived::set_bar() ...
// are not composable.
//
// Let's try static polymorphism (so I finally have an an excuse for curiously
// recurring templates).
template<typename T>
class VolumeConfigParameters
{
private:
    friend T;

    VolumeConfigParameters(const VolumeId& id,
                           const backend::Namespace& nspace,
                           const OwnerTag owner_tag)
        : volume_id_(id)
        , nspace_(nspace)
        , owner_tag_(owner_tag)
        , metadata_backend_config_(new TCBTMetaDataBackendConfig())
    {}

public:

#define C(x) \
    x(other.x)

#define M(x) \
    x(std::move(other.x))

    VolumeConfigParameters(const VolumeConfigParameters& other)
        : C(volume_id_)
        , C(nspace_)
        , C(owner_tag_)
        , C(size_)
        , C(lba_size_)
        , C(cluster_multiplier_)
        , C(sco_multiplier_)
        , C(tlog_multiplier_)
        , C(max_non_disposable_factor_)
        , metadata_backend_config_(other.metadata_backend_config_->clone())
        , C(parent_nspace_)
        , C(parent_snapshot_)
        , C(volume_role_)
        , C(cluster_cache_enabled_)
        , C(cluster_cache_behaviour_)
        , C(cluster_cache_mode_)
        , C(cluster_cache_limit_)
        , C(metadata_cache_capacity_)
    {}

    VolumeConfigParameters(VolumeConfigParameters&& other)
        : M(volume_id_)
        , M(nspace_)
        , M(owner_tag_)
        , M(size_)
        , M(lba_size_)
        , M(cluster_multiplier_)
        , M(sco_multiplier_)
        , M(tlog_multiplier_)
        , M(max_non_disposable_factor_)
        , M(metadata_backend_config_)
        , M(parent_nspace_)
        , M(parent_snapshot_)
        , M(volume_role_)
        , M(cluster_cache_enabled_)
        , M(cluster_cache_behaviour_)
        , M(cluster_cache_mode_)
        , M(cluster_cache_limit_)
        , M(metadata_cache_capacity_)
    {}

#undef M
#undef C

    VolumeConfigParameters&
    operator=(const VolumeConfigParameters&) = delete;

    ~VolumeConfigParameters() = default;

#define PARAM(type, name)                       \
                                                \
public:                                         \
    const type&                                 \
    get_ ## name() const                        \
    {                                           \
        return name ## _;                       \
    }                                           \
                                                \
private:                                        \
    type name ## _

#define OPTIONAL_PARAM(type, name)              \
    PARAM(boost::optional<type>, name)

    PARAM(VolumeId, volume_id);
    PARAM(backend::Namespace, nspace);
    PARAM(OwnerTag, owner_tag);
    PARAM(VolumeSize, size) = VolumeSize(0);
    PARAM(LBASize, lba_size) = VolumeConfig::default_lba_size();
    PARAM(ClusterMultiplier, cluster_multiplier) = VolumeConfig::default_cluster_multiplier();
    PARAM(SCOMultiplier, sco_multiplier) = VolumeConfig::default_sco_multiplier();
    OPTIONAL_PARAM(TLogMultiplier, tlog_multiplier) = boost::none;
    OPTIONAL_PARAM(SCOCacheNonDisposableFactor, max_non_disposable_factor) = boost::none;
    PARAM(VolumeConfig::MetaDataBackendConfigPtr, metadata_backend_config);
    OPTIONAL_PARAM(backend::Namespace, parent_nspace);
    OPTIONAL_PARAM(SnapshotName, parent_snapshot);
    OPTIONAL_PARAM(VolumeConfig::WanBackupVolumeRole, volume_role);
    PARAM(bool, cluster_cache_enabled) = true; // OPTIONAL_PARAM?
    OPTIONAL_PARAM(ClusterCacheBehaviour, cluster_cache_behaviour);
    OPTIONAL_PARAM(ClusterCacheMode, cluster_cache_mode);
    OPTIONAL_PARAM(ClusterCount, cluster_cache_limit);
    OPTIONAL_PARAM(uint32_t, metadata_cache_capacity);

#undef OPTIONAL_PARAM
#undef PARAM
};

#define SETTER(name)                                                    \
    template<typename T>                                                \
    auto                                                                \
    name(const T& v) -> decltype(*this)                                 \
    {                                                                   \
        name ## _ = v;                                                  \
        return *this;                                                   \
    }                                                                   \
                                                                        \
    template<typename T>                                                \
    auto                                                                \
    name(T&& v) -> decltype(*this)                                      \
    {                                                                   \
        name ## _ = std::move(v);                                       \
        return *this;                                                   \
    }


struct VanillaVolumeConfigParameters
    : public VolumeConfigParameters<VanillaVolumeConfigParameters>
{
    VanillaVolumeConfigParameters(const VolumeId& id,
                                  const backend::Namespace& nspace,
                                  const VolumeSize size,
                                  const OwnerTag owner_tag)
        : VolumeConfigParameters(id,
                                 nspace,
                                 owner_tag)
    {
        size_ = size;
        cluster_cache_enabled_ = true;
    }

    VanillaVolumeConfigParameters(const VanillaVolumeConfigParameters& other)
        : VolumeConfigParameters(other)
    {}

    VanillaVolumeConfigParameters(VanillaVolumeConfigParameters&& other)
        : VolumeConfigParameters(std::move(other))
    {}

    VanillaVolumeConfigParameters&
    operator=(const VanillaVolumeConfigParameters&) = delete;

    ~VanillaVolumeConfigParameters() = default;

    SETTER(lba_size);
    SETTER(cluster_multiplier);
    SETTER(cluster_cache_enabled);
    SETTER(cluster_cache_behaviour);
    SETTER(cluster_cache_mode);
    SETTER(cluster_cache_limit);
    SETTER(sco_multiplier);
    SETTER(tlog_multiplier);
    SETTER(max_non_disposable_factor);
    SETTER(metadata_cache_capacity);
    SETTER(metadata_backend_config);
};

struct CloneVolumeConfigParameters
    : public VolumeConfigParameters<CloneVolumeConfigParameters>
{
    CloneVolumeConfigParameters(const VolumeId& id,
                                const backend::Namespace& nspace,
                                const backend::Namespace& parent_nspace,
                                const OwnerTag owner_tag)
        : VolumeConfigParameters(id,
                                 nspace,
                                 owner_tag)
    {
        parent_nspace_ = parent_nspace;
    }

    CloneVolumeConfigParameters(const CloneVolumeConfigParameters& other)
        : VolumeConfigParameters(other)
    {}

    CloneVolumeConfigParameters(CloneVolumeConfigParameters&& other)
        : VolumeConfigParameters(std::move(other))
    {}

    CloneVolumeConfigParameters&
    operator=(const CloneVolumeConfigParameters&) = default;

    ~CloneVolumeConfigParameters() = default;

    SETTER(parent_snapshot);
    SETTER(cluster_cache_enabled);
    SETTER(cluster_cache_behaviour);
    SETTER(cluster_cache_mode);
    SETTER(cluster_cache_limit);
    SETTER(metadata_cache_capacity);
    SETTER(metadata_backend_config);
};

struct WriteOnlyVolumeConfigParameters
    : public VolumeConfigParameters<WriteOnlyVolumeConfigParameters>
{
    WriteOnlyVolumeConfigParameters(const VolumeId& id,
                                    const backend::Namespace& nspace,
                                    const VolumeSize size,
                                    const VolumeConfig::WanBackupVolumeRole role,
                                    const OwnerTag owner_tag)
        : VolumeConfigParameters(id,
                                 nspace,
                                 owner_tag)
    {
        size_ = size;
        volume_role_ = role;
        cluster_cache_enabled_ = false;
    }

    WriteOnlyVolumeConfigParameters(const WriteOnlyVolumeConfigParameters& other)
        : VolumeConfigParameters(other)
    {}

    WriteOnlyVolumeConfigParameters(WriteOnlyVolumeConfigParameters&& other)
        : VolumeConfigParameters(std::move(other))
    {}

    WriteOnlyVolumeConfigParameters&
    operator=(const WriteOnlyVolumeConfigParameters&) = default;

    ~WriteOnlyVolumeConfigParameters() = default;

    SETTER(lba_size);
    SETTER(cluster_multiplier);
    SETTER(sco_multiplier);
    SETTER(tlog_multiplier);
    SETTER(max_non_disposable_factor);
};

#undef SETTER

}

#endif // !VD_VOLUME_CONFIG_PARAMETERS_H_
