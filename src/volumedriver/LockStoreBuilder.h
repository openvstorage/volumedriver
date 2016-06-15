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
