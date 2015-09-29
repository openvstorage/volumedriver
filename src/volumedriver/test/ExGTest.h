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

#ifndef EXGTEST_H_
#define EXGTEST_H_

#include <stdexcept>

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include <gtest/gtest.h>

#include <youtils/IOException.h>

namespace volumedriver
{

// TODO: this is not the best approach; please remake sometime by overriding the calling of the testbody
// directly, which will require some more subtle changes to google test

class VolumeDriverTestConfig
{
public:
    VolumeDriverTestConfig(bool useClusterCache = false)
         : useClusterCache_(useClusterCache)
    {};

    bool useClusterCache() const
    {
        return useClusterCache_;
    }

private:
    bool useClusterCache_;
};

// Z42: rename ExGTest to VolumeDriverTest
class ExGTest :
        public testing::TestWithParam<VolumeDriverTestConfig>
{
protected:
    ExGTest()
        : catch_(true)
    {}

    void dontCatch()
    {
        catch_ = false;
    }

    virtual void Run()
    {
        if (catch_)
        {
            try
            {
                testing::Test::Run();
            }
            catch (fungi::IOException &e)
            {
                TearDown();
                FAIL() << "IOException: " << e.what();
            }
            catch (std::exception &e)
            {
                TearDown();
                FAIL() << "exception: " << e.what();
            }
            catch (...)
            {
                TearDown();
                FAIL() << "unknown exception";
            }
        }
        else
        {
            testing::Test::Run();
        }
    }

private:
    bool catch_;

    static unsigned xmlrpcport_;

public:
    static boost::filesystem::path
    getTempPath(const std::string& ipath);
};

}

#endif /* EXGTEST_H_ */

// Local Variables: **
// mode: c++ **
// End: **
