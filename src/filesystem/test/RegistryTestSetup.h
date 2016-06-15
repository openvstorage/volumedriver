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

#ifndef VFS_REGISTRY_TEST_SETUP_H_
#define VFS_REGISTRY_TEST_SETUP_H_

#include <boost/property_tree/ptree_fwd.hpp>

#include <youtils/ArakoonTestSetup.h>
#include <youtils/TestBase.h>

#include "../Registry.h"

namespace volumedriverfstest
{

class RegistryTestSetup
    : public youtilstest::TestBase
    , public arakoon::ArakoonTestSetup
{
protected:
    explicit RegistryTestSetup(const std::string& name);

    virtual ~RegistryTestSetup() = default;

    virtual void
    SetUp() override;

    virtual void
    TearDown() override;

    const boost::filesystem::path root_;
    std::shared_ptr<volumedriverfs::Registry> registry_;
};

}

#endif // !VFS_REGISTRY_TEST_SETUP_H_
