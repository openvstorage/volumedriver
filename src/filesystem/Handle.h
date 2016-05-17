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

#ifndef VFS_HANDLE_H_
#define VFS_HANDLE_H_

#include "DirectoryEntry.h"
#include "FastPathCookie.h"
#include "FrontendPath.h"
#include "Object.h"

#include <boost/make_shared.hpp>

#include <youtils/Logging.h>

namespace volumedriverfs
{

// TODO: create subclasses for file-like and directory objects and add
// read/write/sync/... methods.
class Handle
{
public:
    using Ptr = std::unique_ptr<Handle>;

    Handle(const FrontendPath& p,
           DirectoryEntryPtr& d)
        : path_(p)
        , dentry_(d)
    {}

    ~Handle() = default;

    Handle(const Handle&) = delete;

    Handle&
    operator=(const Handle&) = delete;

    const FrontendPath&
    path() const
    {
        return path_;
    }

    DirectoryEntryPtr
    dentry() const
    {
        return dentry_;
    }

    FastPathCookie
    cookie() const
    {
        boost::lock_guard<decltype(cookie_lock_)> g(cookie_lock_);
        return cookie_;
    }

    void
    update_cookie(FastPathCookie c)
    {
        boost::lock_guard<decltype(cookie_lock_)> g(cookie_lock_);
        cookie_ = c;
    }

private:
    DECLARE_LOGGER("VFSHandle");

    FrontendPath path_;
    DirectoryEntryPtr dentry_;

    mutable fungi::SpinLock cookie_lock_;
    FastPathCookie cookie_;
};

}

#endif // !VFS_HANDLE_H_
