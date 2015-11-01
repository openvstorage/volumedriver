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
