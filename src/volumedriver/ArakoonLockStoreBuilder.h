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
