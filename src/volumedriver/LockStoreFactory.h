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
