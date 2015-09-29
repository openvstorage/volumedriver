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

#ifndef BACKEND_SOURCE_INTERFACE_H_
#define BACKEND_SOURCE_INTERFACE_H_

#include <iosfwd>
#include <boost/iostreams/positioning.hpp> // stream_offset

#include "BackendConnectionInterface.h"

namespace backend
{

namespace ios = boost::iostreams;

class BackendSourceInterface
{
public:
    virtual ~BackendSourceInterface() {};

    // add open(BackendConnPtr) to enforce passing conn ownership at this level
    // isof at the implementation level only?

    virtual std::streamsize
    read(char* s,
         std::streamsize n) = 0;

    virtual std::streampos
    seek(ios::stream_offset off,
         std::ios_base::seekdir way) = 0;

    virtual void
    close() = 0;
};

}

#endif // !BACKEND_SOURCE_INTERFACE_H_

// Local Variables: **
// mode: c++ **
// End: **
