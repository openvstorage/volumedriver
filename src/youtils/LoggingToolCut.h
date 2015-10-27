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

#ifndef _LOGGING_TOOLCUT_H
#define _LOGGING_TOOLCUT_H
#include "Logger.h"

namespace toolcut
{
class LoggingToolCut
{
public:
    LoggingToolCut(const std::string& name)
        :severity_logger(new youtils::SeverityLoggerWithName(name))
        , name_(name)
    {}

    void
    operator()(const youtils::Severity,
               const std::string&);

    const std::string&
    name() const;

private:
    std::shared_ptr<youtils::SeverityLoggerWithName> severity_logger;
    const std::string name_;
};

}

#endif // _LOGGING_TOOLCUT_H
