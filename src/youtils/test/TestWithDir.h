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

#ifndef TEST_WITH_DIR_H_
#define TEST_WITH_DIR_H_

#include "../TestBase.h"

namespace youtilstest
{

class TestWithDir
    : public TestBase
{
public:
    TestWithDir(const std::string& dirName);
    TestWithDir();

    const boost::filesystem::path&
    getDir();

protected:
    virtual void
    SetUp();

    virtual void
    TearDown();

private:
    boost::filesystem::path directory_;
};
}
#endif // TEST_WITH_DIR_H_
