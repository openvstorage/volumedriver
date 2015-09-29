// Copyright 2015 Open vStorage NV
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
