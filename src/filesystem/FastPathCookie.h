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

#ifndef VFS_FAST_PATH_COOKIE_H_
#define VFS_FAST_PATH_COOKIE_H_

#include <volumedriver/VolumeFwd.h>

namespace volumedriverfs
{

// OUR_STRONG_TYPEDEFs would be nice but we ain't got no operator== for weak_ptr.
// No support for local files for now. Look into boost::variant when going there.
struct LocalVolumeCookie
{
    volumedriver::WeakVolumePtr volume;

    explicit LocalVolumeCookie(const volumedriver::WeakVolumePtr& v)
        : volume(v)
    {}
};

using FastPathCookie = std::shared_ptr<LocalVolumeCookie>;

}

#endif // !VFS_FAST_PATH_COOKIE_H_
