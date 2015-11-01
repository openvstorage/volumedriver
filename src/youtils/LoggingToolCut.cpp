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

#include "LoggingToolCut.h"

#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_feature.hpp>

namespace toolcut
{
void
LoggingToolCut::operator()(const youtils::Severity sev,
                           const std::string& str)
{
    if (youtils::Logger::filter(severity_logger->name,
                                sev))
    {
        BOOST_LOG_SEV(severity_logger->get(), sev) << str;
    }
}

const std::string&
LoggingToolCut::name() const
{
    return name_;
}

}
