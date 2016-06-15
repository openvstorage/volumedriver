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
