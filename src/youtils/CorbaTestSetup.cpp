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
