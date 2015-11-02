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

#ifndef VD_ARAKOON_LOCK_STORE_BUILDER_H_
#define VD_ARAKOON_LOCK_STORE_BUILDER_H_

#include "LockStoreBuilder.h"
#include "VolumeDriverParameters.h"

#include <youtils/ArakoonLockStore.h>
#include <youtils/LockedArakoon.h>
#include <youtils/Logging.h>

namespace volumedriver
{

class ArakoonLockStoreBuilder
    : public LockStoreBuilder
{
public:
    explicit ArakoonLockStoreBuilder(const boost::property_tree::ptree& pt);

    ~ArakoonLockStoreBuilder() = default;

    ArakoonLockStoreBuilder(const ArakoonLockStoreBuilder&) = default;

    ArakoonLockStoreBuilder&
    operator=(const ArakoonLockStoreBuilder&) = default;

    youtils::GlobalLockStorePtr
    build_one(const backend::Namespace& nspace) override final;

    void
    update(const boost::property_tree::ptree&,
           youtils::UpdateReport&) override final;

    void
    persist(boost::property_tree::ptree&,
            const ReportDefault) const override final;

    bool
    checkConfig(const boost::property_tree::ptree&,
                youtils::ConfigurationReport&) const override final;

    virtual const char*
    componentName() const override final
    {
        static const char n[] = "arakoon_lock_store";
        return n;
    }

private:
    DECLARE_LOGGER("ArakoonLockStoreBuilder");

    DECLARE_PARAMETER(dls_arakoon_cluster_id);
    DECLARE_PARAMETER(dls_arakoon_cluster_nodes);
    DECLARE_PARAMETER(dls_arakoon_timeout_ms);

    std::shared_ptr<youtils::LockedArakoon> larakoon_;
};

}

#endif //!VD_ARAKOON_LOCK_STORE_BUILDER_H_
