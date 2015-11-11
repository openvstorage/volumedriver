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

#include "ScrubReply.h"

namespace scrubbing
{

namespace
{

using IArchive = boost::archive::xml_iarchive;
using OArchive = boost::archive::xml_oarchive;

const char archive_name[] = "scrubreply";

}

ScrubReply::ScrubReply(const std::string& s)
{
    std::istringstream is(s);
    IArchive ia(is);
    ia & boost::serialization::make_nvp(archive_name,
                                        *this);
}

std::string
ScrubReply::str() const
{
    std::ostringstream os;
    OArchive oa(os);
    oa & boost::serialization::make_nvp(archive_name,
                                        *this);
    return os.str();
}

}
