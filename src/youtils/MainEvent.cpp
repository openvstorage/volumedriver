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

#include "MainEvent.h"

#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_feature.hpp>

namespace youtils
{

MainEvent::MainEvent(const std::string& what,
                     Logger::logger_type& logger)
    : what_(what)
    , logger_(logger)
{
    BOOST_LOG_SEV(logger_.get(),
                  youtils::Severity::notification) << what_ << ", START" ;
}

MainEvent::~MainEvent()
{
    if(std::uncaught_exception())
    {
        try
        {
            BOOST_LOG_SEV(logger_.get(),
                          youtils::Severity::notification) << what_ << ", EXCEPTION";
        }
        catch(...)
        {}

    }
    else
    {
        BOOST_LOG_SEV(logger_.get(),
                      youtils::Severity::notification) << what_  << ", FINISHED";
    }
}

}
