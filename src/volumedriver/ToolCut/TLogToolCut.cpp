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

#include "TLogToolCut.h"

namespace toolcut
{

namespace fs = boost::filesystem;
namespace yt = youtils;

TLogToolCut::TLogToolCut(const vd::TLog& tlog)
    : tlog_(tlog)
{}

bool
TLogToolCut::writtenToBackend() const
{
    return tlog_.writtenToBackend();
}

std::string
TLogToolCut::getName() const
{
    return tlog_.getName();
}

std::string
TLogToolCut::getUUID() const
{
    return static_cast<yt::UUID>(tlog_.id()).str();
}

bool
TLogToolCut::isTLogString(const std::string& in)
{
    return vd::TLog::isTLogString(in);
}

std::string
TLogToolCut::getUUIDFromTLogName(const std::string& tlogName)
{
    return static_cast<yt::UUID>(boost::lexical_cast<vd::TLogId>(tlogName)).str();
}

std::string
TLogToolCut::str() const
{
    std::stringstream ss;
    ss << "name: " << getName() << std::endl;
    ss << "writtenToBackend: " << writtenToBackend() << std::endl;
    ss << "uuid: " << getUUID() << std::endl;
    return ss.str();
}

std::string
TLogToolCut::repr() const
{
    return std::string("< TLog \n") + str() + "\n>";
}

}

// Local Variables: **
// mode: c++ **
// End: **
