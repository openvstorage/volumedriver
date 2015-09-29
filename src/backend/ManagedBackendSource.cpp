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

#include "ManagedBackendSource.h"
#include "BackendSourceInterface.h"
#include "BackendConnectionManager.h"

namespace backend
{

class ManagedBackendSourceImpl
{
public:
    ManagedBackendSourceImpl(BackendConnectionManagerPtr cm,
                             const Namespace& nspace,
                             const std::string& name)
        : cm_(cm)
        , source_(cm_->newBackendSource(nspace, name))
    {}

    ManagedBackendSourceImpl(const ManagedBackendSourceImpl&) = delete;

    ManagedBackendSourceImpl&
    operator=(const ManagedBackendSourceImpl&) = delete;

    std::streamsize
    read(char* s,
          std::streamsize n)
    {
        return source_->read(s, n);
    }

    std::streampos
    seek(ios::stream_offset off,
         std::ios_base::seekdir way)
    {
        return source_->seek(off, way);
    }

    void
    close()
    {
        source_->close();
    }

private:
    BackendConnectionManagerPtr cm_;
    std::unique_ptr<BackendSourceInterface> source_;
};

ManagedBackendSource::ManagedBackendSource(BackendConnectionManagerPtr cm,
                                           const Namespace& nspace,
                                           const std::string& name)
    : impl_(new ManagedBackendSourceImpl(cm, nspace, name))
{}

std::streamsize
ManagedBackendSource::read(char* s,
                           std::streamsize n)
{
    return impl_->read(s, n);
}

std::streampos
ManagedBackendSource::seek(ios::stream_offset off,
                           std::ios_base::seekdir way)
{
    return impl_->seek(off, way);
}

void
ManagedBackendSource::close()
{
    impl_->close();
}

}
// Local Variables: **
// mode: c++ **
// End: **
