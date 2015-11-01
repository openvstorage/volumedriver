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

#include <youtils/TestBase.h>
#include <youtils/Logging.h>
#include <youtils/UUID.h>

#include <boost/lexical_cast.hpp>

#include "../BackendParameters.h"

namespace backendtest
{

namespace be = backend;
namespace yt = youtils;

class BackendParametersTest
    : public youtilstest::TestBase
{
protected:
    template<typename T>
    void
    check_streaming(T t, const std::string& s)
    {
        std::stringstream ss;

        ss << t;
        EXPECT_TRUE(ss.good());
        EXPECT_EQ(s, ss.str());

        {
            T u;
            ss >> u;
            EXPECT_TRUE(not ss.fail());
            EXPECT_EQ(t, u);
        }

        const T u(boost::lexical_cast<T>(s));
        EXPECT_EQ(t, u);
    }

    template<typename T>
    void
    check_error_handling()
    {
        const std::string s(yt::UUID().str());
        std::stringstream ss;
        ss << s;
        EXPECT_TRUE(ss.good());

        T t;
        ss >> t;
        EXPECT_TRUE(ss.fail());

        EXPECT_THROW(boost::lexical_cast<T>(s),
                     std::exception);
    }

private:
    DECLARE_LOGGER("BackendParametersTest");
};

TEST_F(BackendParametersTest, backend_type_streaming)
{
    //    check_streaming(be::BackendType::BUCHLA, "BUCHLA");
    check_streaming(be::BackendType::LOCAL, "LOCAL");
    check_streaming(be::BackendType::S3, "S3");

    check_error_handling<be::BackendType>();
}

TEST_F(BackendParametersTest, s3_flavour_streaming)
{
    check_streaming(be::S3Flavour::S3, "S3");
    check_streaming(be::S3Flavour::GCS, "GCS");
    check_streaming(be::S3Flavour::WALRUS, "WALRUS");
    check_streaming(be::S3Flavour::SWIFT, "SWIFT");

    check_error_handling<be::S3Flavour>();
}


}
