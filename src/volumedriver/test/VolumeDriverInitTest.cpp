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

#include "ExGTest.h"
#include "../VolManager.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace volumedrivertest
{
using namespace volumedriver;

class VolumeDriverInitTest : public ExGTest
{};


TEST_F(VolumeDriverInitTest, initialize)
{
    fs::path initfile("/tmp/volmanager_init_file");
    if(fs::exists(initfile))
    {
        boost::property_tree::ptree pt;
        try
        {
            boost::property_tree::json_parser::read_json(initfile.string(),
                                                         pt);
        }
        catch(std::exception& e)
        {
            ASSERT_TRUE(false) << "Could  not read property tree, " << e.what();
            return;

        }
        catch(...)
        {
            ASSERT_TRUE(false) << "Could not read property tree, unknown exception";
            return;

        }

        try
        {
            VolManager::start(pt);
        }
        catch(std::exception& e)
        {
            ASSERT_TRUE(false) << "Could not initialize VolManager, " << e.what();
            return;

        }
        catch(...)
        {
            ASSERT_TRUE(false) << "Could not initialize VolManager, unknown exception " ;
            return;
        }
        VolManager::stop();

    }
}
}

// Local Variables: **
// mode: c++ **
// End: **
