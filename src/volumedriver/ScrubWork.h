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

#ifndef _SCRUBWORK_H_
#define _SCRUBWORK_H_

#include <string>
#include <youtils/Serialization.h>
#include <backend/BackendConfig.h>
#include "Types.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>


namespace scrubbing
{
struct ScrubWork
{
    ScrubWork(std::unique_ptr<backend::BackendConfig>&& backend_config,
              const volumedriver::Namespace& ns,
              const volumedriver::VolumeId& id,
              const volumedriver::ClusterExponent cluster_exponent,
              const uint32_t sco_size,
              const std::string& snapshot_name)
        : backend_config_(std::move(backend_config))
        , ns_(ns)
        , id_(id)
        , cluster_exponent_(cluster_exponent)
        , sco_size_(sco_size)
        , snapshot_name_(snapshot_name)
    {}

    ScrubWork()
        : ns_()
    {}

    ScrubWork(const std::string& in)
        : ns_()
    {
        std::stringstream iss(in);
        ScrubWork::iarchive_type ia(iss);
        ia & boost::serialization::make_nvp("scrubwork",
                                            *this);
    }

    std::string
    str() const
    {
        std::stringstream oss;
        ScrubWork::oarchive_type oa(oss);
        oa & boost::serialization::make_nvp("scrubwork",
                                            *this);
        return oss.str();
    }

    DECLARE_LOGGER("ScrubWork");

    typedef boost::archive::xml_iarchive iarchive_type;
    typedef boost::archive::xml_oarchive oarchive_type;

    std::unique_ptr<backend::BackendConfig> backend_config_;
    volumedriver::Namespace ns_;
    volumedriver::VolumeId id_;
    volumedriver::ClusterExponent cluster_exponent_;
    uint32_t sco_size_;
    std::string snapshot_name_;

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    inline void save(Archive & ar,
                     const unsigned int version) const
    {
        VERIFY(backend_config_.get());
        if(version == 1)
        {
            boost::property_tree::ptree pt;
            backend_config_->persist_internal(pt,
                                              ReportDefault::T);
            std::stringstream ss;
            write_json(ss,
                       pt);

            std::string backend_config = ss.str();

            ar & BOOST_SERIALIZATION_NVP(backend_config);
            ar & BOOST_SERIALIZATION_NVP(ns_);
            ar & BOOST_SERIALIZATION_NVP(id_);
            ar & BOOST_SERIALIZATION_NVP(cluster_exponent_);
            ar & BOOST_SERIALIZATION_NVP(sco_size_);
            ar & BOOST_SERIALIZATION_NVP(snapshot_name_);
        }
        else
        {
            throw youtils::SerializationVersionException("ScrubWork",
                                                         version,
                                                         1,
                                                         1);
        }
    }

    template<class Archive>
    void
    load(Archive& ar,
         const unsigned int version)
    {
        if(version == 1)
        {
            std::string backend_config;
            ar & BOOST_SERIALIZATION_NVP(backend_config);
            backend_config_ = backend::BackendConfig::makeBackendConfig(backend_config);

            ar & BOOST_SERIALIZATION_NVP(ns_);
            ar & BOOST_SERIALIZATION_NVP(id_);
            ar & BOOST_SERIALIZATION_NVP(cluster_exponent_);
            ar & BOOST_SERIALIZATION_NVP(sco_size_);
            ar & BOOST_SERIALIZATION_NVP(snapshot_name_);
        }
        else
        {
            throw youtils::SerializationVersionException("ScrubWork",
                                                         version,
                                                         1,
                                                         1);
        }
    }
};

}
BOOST_CLASS_VERSION(scrubbing::ScrubWork,1);
#endif // _SCRUBWORK_H_
