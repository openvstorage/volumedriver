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

#include "BackendTestBase.h"

#include <youtils/CheckSum.h>

#include <backend/BackendConnectionInterface.h>

namespace backend
{

namespace yt = youtils;

class ExtendedApiTest
    : public BackendTestBase
{
public:
    ExtendedApiTest()
        : BackendTestBase("ExtendedApiTest")
    {}

    virtual void
    SetUp()
    {
        BackendTestBase::SetUp();
        nspace_ = make_random_namespace();

        // BackendConnPtr c(cm_->getConnection(false));
        // ASSERT_TRUE(c->hasExtendedApi())
        //     << "these tests require a backend that supports the extended api";
    }
    virtual void
        TearDown()
    {
        nspace_.reset();
        BackendTestBase::TearDown();
    }

    std::unique_ptr<WithRandomNamespace> nspace_;

};

TEST_F(ExtendedApiTest,
       x_write_checksum)
{
    {
        BackendConnectionInterfacePtr c(cm_->getConnection());
        if (not c->hasExtendedApi())
        {
            SUCCEED() << "these tests require a backend that supports the extended api";
            return;
        }
    }


    const std::vector<char> buf(7, 'x');
    yt::CheckSum chksum;
    chksum.update(&buf[0], buf.size());

    BackendConnectionInterfacePtr c(cm_->getConnection());
    c->timeout(boost::posix_time::seconds(1));
    const std::string oname("testobject");
    ObjectInfo oi(c->x_write(nspace_->ns(),
                             std::string(&buf[0], buf.size()),
                             oname));

    EXPECT_EQ(buf.size(), oi.size_);
    EXPECT_EQ(chksum, oi.checksum_);
}

}

// Local Variables: **
// End: **
