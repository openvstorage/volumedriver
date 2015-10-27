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

#ifndef CORBA_TEST_SETUP_H_
#define CORBA_TEST_SETUP_H_
#include <boost/filesystem.hpp>
#include "pstream.h"
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
