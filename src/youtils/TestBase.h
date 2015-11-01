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

#ifndef TESTBASE_H_
#define TESTBASE_H_

#include <gtest/gtest.h>
#include "IOException.h"
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

// Streaming out optionals

template <typename T>
std::ostream&
operator<<(std::ostream& os,
           const boost::optional<T>& val)
{
    if(val)
    {
        return os << val;
    }
    else
    {

        return os << "None";
    }

}

namespace youtilstest
{
namespace fs = boost::filesystem;

class TestBase
    : public testing::Test
{
protected:
    virtual void
    Run();

public:
    static boost::filesystem::path
    getTempPath(const std::string& ipath);
};


}

#endif /* TESTSBASE_H_ */

// Local Variables: **
// mode: c++ **
// End: **
