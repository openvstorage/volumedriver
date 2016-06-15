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
