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

#ifndef BACKEND_TEST_BASE_H_
#define BACKEND_TEST_BASE_H_

#include <youtils/TestBase.h>
#include "../BackendInterface.h"
#include "../BackendTestSetup.h"

namespace backend
{

// convenient base class for Backend*Test.cpp
// should probably switch names with BackendTestSetup
class BackendTestBase
    : public youtilstest::TestBase
    , public BackendTestSetup
{
    DECLARE_LOGGER("BackendTestBase");

protected:
    explicit BackendTestBase(const std::string& test_name);

    virtual void
    SetUp();

    virtual void
    TearDown();

    bool
    retrieveAndVerify(const boost::filesystem::path& p,
                      size_t size,
                      const std::string& pattern,
                      const youtils::CheckSum* chksum,
                      BackendConnectionManagerPtr bcm,
                      const std::string& objname,
                      const Namespace& );

    // Not ThreadSafe
    youtils::CheckSum
    createAndPut(const boost::filesystem::path& p,
                 size_t size,
                 const std::string& pattern,
                 BackendConnectionManagerPtr bc,
                 const std::string& objname,
                 const Namespace& ns,
                 const OverwriteObject overwrite);

    youtils::CheckSum
    createPutAndVerify(const boost::filesystem::path& p,
                       size_t size,
                       const std::string& pattern,
                       BackendConnectionManagerPtr bc,
                       const std::string& objname,
                       const Namespace& ns,
                       const OverwriteObject overwrite);

    youtils::CheckSum
    createTestFile(const boost::filesystem::path& p,
                   size_t size,
                   const std::string& pattern);

    BackendInterfacePtr
    bi_(const Namespace& ns)
    {
        return createBackendInterface(ns);
    }

    const boost::filesystem::path path_;

    bool
    verifyTestFile(const boost::filesystem::path& p,
                   size_t expected_size,
                   const std::string& pattern,
                   const youtils::CheckSum* expected_chksum);

};

}

#endif // !BACKEND_TEST_BASE_H_

// Local Variables: **
// mode: c++ **
// End: **
