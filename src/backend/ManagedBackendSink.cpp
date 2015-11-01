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

#include <boost/utility.hpp>

#include "ManagedBackendSink.h"
#include "BackendSinkInterface.h"
#include "BackendConnectionManager.h"

namespace backend
{

class ManagedBackendSinkImpl
{
public:
    ManagedBackendSinkImpl(BackendConnectionManagerPtr cm,
                           const Namespace& nspace,
                           const std::string& name)
        : cm_(cm)
        , sink_(cm_->newBackendSink(nspace, name))
    {}

    ManagedBackendSinkImpl(const ManagedBackendSinkImpl&) = delete;

    ManagedBackendSinkImpl&
    operator=(const ManagedBackendSinkImpl&) = delete;

    std::streamsize
    write(const char* s,
          std::streamsize n)
    {
        return sink_->write(s, n);
    }

    void
    close()
    {
        sink_->close();
    }

private:
    BackendConnectionManagerPtr cm_;
    std::unique_ptr<BackendSinkInterface> sink_;
};

ManagedBackendSink::ManagedBackendSink(BackendConnectionManagerPtr cm,
                                       const Namespace& nspace,
                                       const std::string& name)
    : impl_(new ManagedBackendSinkImpl(cm, nspace, name))
{}

std::streamsize
ManagedBackendSink::write(const char* s,
                          std::streamsize n)
{
    return impl_->write(s, n);
}

void
ManagedBackendSink::close()
{
    impl_->close();
}

}
// Local Variables: **
// mode: c++ **
// End: **
