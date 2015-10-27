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

#ifndef TEST_WITH_DIR_H_
#define TEST_WITH_DIR_H_
#include "../TestBase.h"

namespace youtilstest
{

class TestWithDir : public TestBase
{
public:
    TestWithDir(const std::string& dirName);
    TestWithDir();
    const fs::path& getDir();

protected:
    virtual void SetUp();
    virtual void TearDown();

private:
  fs::path directory_;
};
}
#endif // TEST_WITH_DIR_H_
// Local Variables: **
// bvirtual-targets: ("target/bin/youtils_test") **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
