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

#ifndef YOUTILS_TRACER_H_
#define YOUTILS_TRACER_H_

#include "Logger.h"
#include "Logging.h"

namespace youtils
{

class Tracer
{
public:
    Tracer(Logger::logger_type& l,
           const std::string& s)
        : l_(l)
        , s_(s)
    {
        BOOST_LOG_SEV(l_.get(), youtils::Severity::trace) << "Enter: " << s_;
    }

    ~Tracer()
    {
        BOOST_LOG_SEV(l_.get(), youtils::Severity::trace) << "Exit: " << s_;
    }

    Logger::logger_type& l_;
    const std::string s_;
};

#ifndef NDEBUG
#define TRACER ::youtils::Tracer _t(getLogger__(), __FUNCTION__)
#else
#define TRACER
#endif

}

#endif // !YOUTILS_TRACER_H_

// Local Variables: **
// mode: c++ **
// End: **
