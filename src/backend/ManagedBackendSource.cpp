// Copyright (C) 2016 iNuron NV
//
// This file is part of Open vStorage Open Source Edition (OSE),
// as available from
//
//      http://www.openvstorage.org and
//      http://www.openvstorage.com.
//
// This file is free software; you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License v3 (GNU AGPLv3)
// as published by the Free Software Foundation, in version 3 as it comes in
// the LICENSE.txt file of the Open vStorage OSE distribution.
// Open vStorage is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY of any kind.

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
