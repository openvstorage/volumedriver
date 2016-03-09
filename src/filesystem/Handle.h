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
