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

#ifndef VD_LOCK_STORE_FACTORY_H_
#define VD_LOCK_STORE_FACTORY_H_

#include "LockStoreBuilder.h"
#include "VolumeDriverParameters.h"

#include <youtils/GlobalLockStore.h>
#include <youtils/Logging.h>

#include <backend/BackendConnectionManager.h>

namespace volumedriver
{

class LockStoreFactory
    : public LockStoreBuilder
{
public:
    LockStoreFactory(const boost::property_tree::ptree&,
                     const RegisterComponent,
                     backend::BackendConnectionManagerPtr);

    ~LockStoreFactory() = default;

    LockStoreFactory(const LockStoreFactory&) = delete;

    LockStoreFactory&
    operator=(const LockStoreFactory&) = delete;

    youtils::GlobalLockStorePtr
    build_one(const backend::Namespace& nspace) override final
    {
        return builder_->build_one(nspace);
    }

    static const char*
    name()
    {
        static const char n[] = "distributed_lock_store";
        return n;
    }

    const char*
    componentName() const override final
    {
        return name();
    }

    void
    update(const boost::property_tree::ptree&,
           youtils::UpdateReport&) override final;

    void
    persist(boost::property_tree::ptree&,
            const ReportDefault) const override final;

    bool
    checkConfig(const boost::property_tree::ptree&,
                youtils::ConfigurationReport&) const override final;

private:
    DECLARE_LOGGER("LockStoreFactory");

    DECLARE_PARAMETER(dls_type);

    std::unique_ptr<LockStoreBuilder> builder_;
};

}

#endif // !VD_NAMESPACE_LOCK_MANAGER_H_
