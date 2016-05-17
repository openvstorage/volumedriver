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
