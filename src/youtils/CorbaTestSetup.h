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

#ifndef CORBA_TEST_SETUP_H_
#define CORBA_TEST_SETUP_H_

#include <boost/filesystem.hpp>
#include <pstreams/pstream.h>

namespace youtils
{
class CorbaTestSetup
{
public:
    CorbaTestSetup(const boost::filesystem::path& binary_path);

    void
    start();

    void
    stop();

    bool
    running();

private:
    const boost::filesystem::path binary_path_;
    std::unique_ptr<redi::ipstream> ipstream_;

};

}

#endif // CORBA_TEST_SETUP_H_
