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

#ifndef MANAGED_BACKEND_SOURCE_H_
#define MANAGED_BACKEND_SOURCE_H_

#include "BackendConnectionManager.h"

#include <iosfwd>

#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/positioning.hpp>
#include <boost/shared_ptr.hpp>

#include <youtils/Logging.h>

namespace backend
{

namespace ios = boost::iostreams;

class ManagedBackendSourceImpl;

// This one needs to be copy-constructible for boost::iostreams, hence
// - copy construction/assignment is not disabled
// - the PIMPL with a shared_ptr
// TODO: investigate whether merging ManagedBackend{Sink,Source} is feasible
class ManagedBackendSource
{
public:
    typedef char char_type;

    struct StreamCategory
        : public ios::input_seekable //source_tag
        , public ios::closable_tag
        , public ios::device_tag
           //, public ios::direct_tag // this breaks compilation - figure out why
    {};

    typedef StreamCategory category;

    ManagedBackendSource(BackendConnectionManagerPtr cm,
            const Namespace& nspace,
            const std::string& name);

    ManagedBackendSource(const ManagedBackendSource&) = default;

    ManagedBackendSource&
    operator=(const ManagedBackendSource&) = default;

    std::streamsize
    read(char* s,
         std::streamsize n);

    std::streampos
    seek(ios::stream_offset off,
         std::ios_base::seekdir way);

    void
    close();

private:
    DECLARE_LOGGER("ManagedBackendSource");

    boost::shared_ptr<ManagedBackendSourceImpl> impl_;
};

}

#endif // MANAGED_BACKEND_SOURCE_H_

// Local Variables: **
// mode: c++ **
// End: **
