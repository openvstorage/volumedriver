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

#ifndef MANAGED_BACKEND_SINK_H_
#define MANAGED_BACKEND_SINK_H_

#include "BackendConnectionManager.h"

#include <iosfwd>
#include <boost/iostreams/categories.hpp>
#include <boost/shared_ptr.hpp>

#include <youtils/Logging.h>

namespace backend
{

namespace ios = boost::iostreams;

class ManagedBackendSinkImpl;

// This one needs to be copy-constructible for boost::iostreams, hence
// - copy construction/assignment is not disabled
// - the PIMPL with a shared_ptr
class ManagedBackendSink
{
public:
    typedef char char_type;

    struct StreamCategory
        : public ios::sink_tag
        , public ios::closable_tag
           //, public ios::direct_tag // this breaks compilation - figure out why
    {};

    typedef StreamCategory category;

    ManagedBackendSink(BackendConnectionManagerPtr cm,
                       const Namespace& nspace,
                       const std::string& name);

    ManagedBackendSink(const ManagedBackendSink&) = default;

    ManagedBackendSink&
    operator=(const ManagedBackendSink&) = default;

    std::streamsize
    write(const char* s,
          std::streamsize n);

    void
    close();

private:
    DECLARE_LOGGER("ManagedBackendSink");

    boost::shared_ptr<ManagedBackendSinkImpl> impl_;
};

}

#endif // !MANAGED_BACKEND_SINK_H_

// Local Variables: **
// mode: c++ **
// End: **
