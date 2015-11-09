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
