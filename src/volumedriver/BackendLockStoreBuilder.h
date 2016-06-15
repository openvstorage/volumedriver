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

#ifndef VD_BACKEND_LOCK_STORE_BUILDER_H_
#define VD_BACKEND_LOCK_STORE_BUILDER_H_

#include "LockStoreBuilder.h"

#include <youtils/Logging.h>

#include <backend/BackendConnectionManager.h>
#include <backend/LockStore.h>

namespace volumedriver
{

class BackendLockStoreBuilder
    : public LockStoreBuilder
{
public:
    BackendLockStoreBuilder(const boost::property_tree::ptree& pt,
                            const backend::BackendConnectionManagerPtr cm)
        : LockStoreBuilder(pt,
                           RegisterComponent::F)
        , cm_(cm)
    {}

    ~BackendLockStoreBuilder() = default;

    BackendLockStoreBuilder(const BackendLockStoreBuilder&) = default;

    BackendLockStoreBuilder&
    operator=(const BackendLockStoreBuilder&) = default;

    youtils::GlobalLockStorePtr
    build_one(const backend::Namespace& nspace) override final
    {
        return
            youtils::GlobalLockStorePtr(new backend::LockStore(cm_->newBackendInterface(nspace)));
    }

    void
    update(const boost::property_tree::ptree&,
           youtils::UpdateReport&) override final
    {}

    void
    persist(boost::property_tree::ptree&,
            const ReportDefault) const override final
    {}

    bool
    checkConfig(const boost::property_tree::ptree&,
                youtils::ConfigurationReport&) const override final
    {
        return true;
    }

    virtual const char*
    componentName() const override final
    {
        static const char n[] = "backend_lock_store";
        return n;
    }

private:
    DECLARE_LOGGER("BackendLockStoreBuilder");

    backend::BackendConnectionManagerPtr cm_;
};

}

#endif //!VD_BACKEND_LOCK_STORE_BUILDER_H_
