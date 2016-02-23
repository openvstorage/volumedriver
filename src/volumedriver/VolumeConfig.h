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

#ifndef VOLUMECONFIG_H_
#define VOLUMECONFIG_H_

// this file is part of the external API
// Z42: hide implementation details

#include "ClusterCacheBehaviour.h"
#include "ClusterCacheMode.h"
#include "ClusterCount.h"
#include "MetaDataBackendConfig.h"
#include "OwnerTag.h"
#include "ParentConfig.h"
#include "SnapshotName.h"
#include "Types.h"

#include <stdint.h>

#include <limits>
#include <string>
#include <atomic>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/optional.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/shared_ptr.hpp>

#include <youtils/ArakoonNodeConfig.h>
#include <youtils/Assert.h>
#include <youtils/EnumUtils.h>
#include <youtils/Serialization.h>

namespace volumedriver
{
class Volume;

class CloneVolumeConfigParameters;
class VanillaVolumeConfigParameters;
class WriteOnlyVolumeConfigParameters;

BOOLEAN_ENUM(IsVolumeTemplate)

class VolumeConfig
{
public:
    template<typename T>
    VolumeConfig(const T& t)
        : id_(t.get_volume_id())
        , ns_(t.get_nspace().str())
        , parent_ns_(t.get_parent_nspace() ?
                     boost::optional<std::string>((*t.get_parent_nspace()).str()) :
                     boost::none)
        , parent_snapshot_(t.get_parent_snapshot() ?
                           *t.get_parent_snapshot() :
                           SnapshotName())
        , lba_size_(t.get_lba_size())
        , lba_count_(t.get_size() / lba_size_)
        , cluster_mult_(t.get_cluster_multiplier())
        , sco_mult_(t.get_sco_multiplier())
        , tlog_mult_(t.get_tlog_multiplier())
		, max_non_disposable_factor_(t.get_max_non_disposable_factor())
        , readCacheEnabled_(t.get_cluster_cache_enabled())
        , wan_backup_volume_role_(t.get_volume_role() ?
                                  *t.get_volume_role() :
                                  WanBackupVolumeRole::WanBackupNormal)
        , cluster_cache_behaviour_(t.get_cluster_cache_behaviour())
        , cluster_cache_mode_(t.get_cluster_cache_mode())
        , cluster_cache_limit_(t.get_cluster_cache_limit())
        , metadata_cache_capacity_(t.get_metadata_cache_capacity())
        , metadata_backend_config_(t.get_metadata_backend_config() ?
                                   t.get_metadata_backend_config()->clone().release() :
                                   new TCBTMetaDataBackendConfig())
        , is_volume_template_(IsVolumeTemplate::F)
        , number_of_syncs_to_ignore_(0)
        , maximum_time_to_ignore_syncs_in_seconds_(0)
        , owner_tag_(t.get_owner_tag())
    {
        VERIFY(sco_mult_ > 0);
        if (tlog_mult_)
        {
            VERIFY(tlog_mult_ > 0);
        }
		if (max_non_disposable_factor_)
		{
			VERIFY(max_non_disposable_factor_ > 0);
		}
        VERIFY(lba_size_ > 0);
        VERIFY(cluster_mult_ > 0);

        const uint64_t csize = getClusterSize();
        VERIFY(csize);

        if ((t.get_size() % csize) != 0)
        {
            LOG_ERROR(id_ << ": size is not an integral multiple of cluster size " <<
                      csize);
            throw fungi::IOException("Volume size is not a multiple of cluster size",
                                     id_.str().c_str());
        }

        verify_();
    }

    typedef boost::archive::text_iarchive iarchive_type;
    typedef boost::archive::text_oarchive oarchive_type;
    static const std::string config_backend_name;

    enum class WanBackupVolumeRole
    {
        WanBackupNormal = 0,
        WanBackupBase = 1,
        WanBackupIncremental = 2
    };

    // AR: get rid of this one - it's only needed for boost::serialization
    VolumeConfig();

    VolumeConfig(const CloneVolumeConfigParameters& params,
                 const VolumeConfig& parent_config);

    VolumeConfig(const VolumeConfig&);

    VolumeConfig&
    operator=(const VolumeConfig&);

    /* Name of this volume, superflous since each volume now has a unique namespace */
    const VolumeId id_;

    /* Namespace of the volume, unique for each volume */
    std::string ns_;

    /* If the volume is a clone this holds the namespace of the clone */
    const boost::optional<std::string> parent_ns_;

public:
    /* If the volume is a clone this holds the snapshot that was cloned */
    const SnapshotName parent_snapshot_;

    /* Size of an LBA typically 512 bytes */
    const LBASize lba_size_;

private:
    /* Number of LBA's in this Volume */
    std::atomic<uint64_t> lba_count_;

public:
    /* Number of LBA's in a cluster typically 8 */
    const ClusterMultiplier cluster_mult_;
    /* Number of clusters in a SCO typically 1024 */
    SCOMultiplier sco_mult_;
    /* Number of SCO's in a TLOG typically 1024 */
    boost::optional<TLogMultiplier> tlog_mult_;
	/* Amount of non-disposable data */
	boost::optional<SCOCacheNonDisposableFactor> max_non_disposable_factor_;

    bool readCacheEnabled_;

    const WanBackupVolumeRole wan_backup_volume_role_;

    boost::optional<ClusterCacheBehaviour> cluster_cache_behaviour_;
    boost::optional<ClusterCacheMode> cluster_cache_mode_;
    boost::optional<ClusterCount> cluster_cache_limit_;

    boost::optional<size_t> metadata_cache_capacity_;

    using MetaDataBackendConfigPtr = std::unique_ptr<MetaDataBackendConfig>;
    MetaDataBackendConfigPtr metadata_backend_config_;

private:
    IsVolumeTemplate is_volume_template_;

public:
    IsVolumeTemplate
    isVolumeTemplate() const
    {
        return is_volume_template_;
    }

    void
    setAsTemplate()
    {
        is_volume_template_ = IsVolumeTemplate::T;
    }

    uint64_t
    getClusterSize() const
    {
        return cluster_mult_ * lba_size_;
    }

    uint64_t
    getSCOSize() const
    {
        return lba_size_ * cluster_mult_ * sco_mult_;
    }

    static constexpr uint32_t default_lba_size_ =  512;

    static inline LBASize
    default_lba_size()
    {
        return LBASize(default_lba_size_);
    }

    static constexpr uint32_t default_cluster_size_ =  4096;

    static inline ClusterSize
    default_cluster_size()
    {
        return ClusterSize(default_cluster_size_);
    }

    static constexpr uint32_t
    default_cluster_multiplier_ =  default_cluster_size_ / default_lba_size_;



    static inline ClusterMultiplier
    default_cluster_multiplier()
    {
        return ClusterMultiplier(default_cluster_size() / default_lba_size());
    }

    static constexpr uint32_t default_sco_multiplier_ =  4096;


    static inline SCOMultiplier
    default_sco_multiplier()
    {
        return SCOMultiplier(default_sco_multiplier_);
    }

    static constexpr uint64_t default_sco_size_ =  default_cluster_size_ * default_sco_multiplier_;

    backend::Namespace
    getNS() const
    {
        return backend::Namespace(ns_);
    }

    void
    changeNamespace(const backend::Namespace& ns)
    {
        ns_ = ns.str();
    }

    MaybeParentConfig
    parent() const
    {
        if (parent_ns_)
        {
            return ParentConfig(backend::Namespace(*parent_ns_),
                                parent_snapshot_);
        }
        else
        {
            return boost::none;
        }
    }

    std::atomic<uint64_t>&
    lba_count()
    {
        return lba_count_;
    }

    const std::atomic<uint64_t>&
    lba_count() const
    {
        return lba_count_;
    }

    uint64_t number_of_syncs_to_ignore_;
    uint64_t maximum_time_to_ignore_syncs_in_seconds_;

    OwnerTag owner_tag_;

private:
    DECLARE_LOGGER("VolumeConfig");

    void
    verify_();

    friend class boost::serialization::access;
    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        if (version <= 11)
        {
            // No backward compatibility for now.
            // The below checks are left in place in case we ever want to change that
            // and serve as documentation.
            THROW_SERIALIZATION_ERROR(version, 11, 15);
        }

        if(version == 4)
        {
            LOG_FATAL("Will not deserialize version 4 on strict orders by bdv");
            THROW_SERIALIZATION_ERROR(version, 9, 5);
        }

        ar & const_cast<VolumeId&>(id_);
        ar & ns_;
        ar & const_cast<boost::optional<std::string>& >(parent_ns_);

        {
            std::string psnap;
            ar & psnap;
            const_cast<SnapshotName&>(parent_snapshot_) = SnapshotName(psnap);
        }

        ar & const_cast<LBASize&>(lba_size_);
        ar & lba_count_;
        ar & const_cast<ClusterMultiplier&>(cluster_mult_);
        ar & const_cast<SCOMultiplier&>(sco_mult_);

        if (version >= 14)
        {
            ar & const_cast<boost::optional<TLogMultiplier>& >(tlog_mult_);
            ar & const_cast<boost::optional<SCOCacheNonDisposableFactor>& >(max_non_disposable_factor_);
        }

        if (version < 10)
        {
            uint64_t sco_cache_min; // min size reserved for the volume in sco cache
            uint64_t sco_cache_max; // max size the volume can use in sco cache

            ar & const_cast<uint64_t&>(sco_cache_min);
            ar & const_cast<uint64_t&>(sco_cache_max);
        }

        if (version >= 1)
        {
            ar & const_cast<bool&>(readCacheEnabled_);
        }

        if(version >= 5)
        {
            ar & const_cast<WanBackupVolumeRole&>(wan_backup_volume_role_);
        }

        if(version >= 6)
        {
            ar & cluster_cache_behaviour_;
        }

        if(version >= 7 and version < 11)
        {
            arakoon::ClusterID cid;
            ar & cid;

            std::list<arakoon::ArakoonNodeConfig> nodel;
            ar & nodel;

            MetaDataBackendConfigPtr cfg;

            if (not nodel.empty())
            {
                const std::vector<arakoon::ArakoonNodeConfig> nodev(nodel.begin(),
                                                                    nodel.end());

                const_cast<MetaDataBackendConfigPtr&>(metadata_backend_config_) =
                    MetaDataBackendConfigPtr(new ArakoonMetaDataBackendConfig(cid,
                                                                              nodev));
            }
        }

        if (version >= 8)
        {
            ar & is_volume_template_;
        }

        if (version >= 11)
        {
            ar & const_cast<MetaDataBackendConfigPtr&>(metadata_backend_config_);
        }
        if (version >= 12)
        {
            ar & number_of_syncs_to_ignore_;
            ar & maximum_time_to_ignore_syncs_in_seconds_;
        }

        if (version >= 13)
        {
            ar & cluster_cache_mode_;
            ar & owner_tag_;
        }
        else
        {
            VERIFY(owner_tag_ == OwnerTag(0));
        }

        if (version >= 14)
        {
            ar & cluster_cache_limit_;
        }

        if (version >= 15)
        {
            ar & metadata_cache_capacity_;
        }

        // cf. comment in constructor.

        Namespace tmp = backend::Namespace(ns_);
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned int version) const
    {
        if (version != 15)
        {
            THROW_SERIALIZATION_ERROR(version, 15, 15);
        }

        ar & id_;
        ar & ns_;
        ar & parent_ns_;
        ar & static_cast<const std::string>(parent_snapshot_);
        ar & lba_size_;
        ar & lba_count_;
        ar & cluster_mult_;
        ar & sco_mult_;
        ar & tlog_mult_;
        ar & max_non_disposable_factor_;
        ar & readCacheEnabled_;
        ar & wan_backup_volume_role_;
        ar & cluster_cache_behaviour_;
        ar & is_volume_template_;
        ar & metadata_backend_config_;
        ar & number_of_syncs_to_ignore_;
        ar & maximum_time_to_ignore_syncs_in_seconds_;
        ar & cluster_cache_mode_;
        ar & owner_tag_;
        ar & cluster_cache_limit_;
        ar & metadata_cache_capacity_;
    }
};

std::istream&
operator>>(std::istream& is,
           volumedriver::VolumeConfig::WanBackupVolumeRole& jt);

std::ostream&
operator<<(std::ostream& os,
           const volumedriver::VolumeConfig::WanBackupVolumeRole& jt);

}

namespace youtils
{

template<>
struct EnumTraits<volumedriver::VolumeConfig::WanBackupVolumeRole>
{
    static const char* strings[];
    static const size_t size;
};

}

BOOST_CLASS_VERSION(volumedriver::VolumeConfig, 15);

#endif /* !VOLUMECONFIG_H_ */

// Local Variables: **
// mode: c++ **
// End: **
