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
