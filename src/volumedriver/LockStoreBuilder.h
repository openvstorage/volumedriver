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

#ifndef VD_LOCK_STORE_BUILDER_H_
#define VD_LOCK_STORE_BUILDER_H_

#include <boost/property_tree/ptree_fwd.hpp>

#include <youtils/BooleanEnum.h>
#include <youtils/ConfigurationReport.h>
#include <youtils/GlobalLockStore.h>
#include <youtils/UpdateReport.h>
#include <youtils/VolumeDriverComponent.h>

#include <backend/Namespace.h>

DECLARE_BOOLEAN_ENUM(ReportDefault);

namespace volumedriver
{

class LockStoreBuilder
    : public youtils::VolumeDriverComponent
{
protected:
    LockStoreBuilder(const boost::property_tree::ptree& pt,
                     const RegisterComponent registerize)
        : youtils::VolumeDriverComponent(registerize,
                                         pt)
    {}

public:
    virtual ~LockStoreBuilder() = default;

    virtual youtils::GlobalLockStorePtr
    build_one(const backend::Namespace&) = 0;
};

}

#endif // !VD_NAMESPACE_LOCK_MANAGER_H_
