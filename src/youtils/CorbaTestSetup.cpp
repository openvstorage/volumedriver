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

#include "CorbaTestSetup.h"

namespace youtils
{
CorbaTestSetup::CorbaTestSetup(const boost::filesystem::path& binary_path)
    : binary_path_(binary_path)
{}

void
CorbaTestSetup::start()
{
    redi::pstreams::argv_type arguments;
    ipstream_ = std::make_unique<redi::ipstream>(binary_path_.string());
}

void
CorbaTestSetup::stop()
{}


bool
CorbaTestSetup::running()
{
    if(not ipstream_)
    {
        return false;
    }
    else
    {
        return not ipstream_->rdbuf()->exited();
    }
}

}
