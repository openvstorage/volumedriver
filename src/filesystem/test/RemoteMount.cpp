// Copyright (C) 2017 iNuron NV
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
// but OUT ANY WARRANTY of any kind.

#include "FileSystemTestBase.h"

#include <youtils/Catchers.h>

namespace volumedriverfstest
{

RemoteMount::RemoteMount(FileSystemTestBase& base)
    : base_(&base)
{
    mount();
}

void
RemoteMount::mount()
{
    THROW_WHEN(mounted_);
    if (base_)
    {
        base_->mount_remote();
        mounted_ = true;
    }
}

void
RemoteMount::umount(bool ignore_errors)
{
    THROW_UNLESS(mounted_);
    if (base_)
    {
        base_->umount_remote(ignore_errors);
        mounted_ = false;
    }
}

RemoteMount::RemoteMount(RemoteMount&& other)
    : base_(other.base_)
    , mounted_(other.mounted_)
{
    other.base_ = nullptr;
    other.mounted_ = false;
}

RemoteMount&
RemoteMount::operator=(RemoteMount&& other)
{
    if (this != &other)
    {
        base_ = other.base_;
        mounted_ = other.mounted_;
        other.base_ = nullptr;
        other.mounted_ = false;
    }

    return *this;
}

RemoteMount::~RemoteMount()
{
    try
    {
        if (mounted_)
        {
            umount();
        }
    }
    CATCH_STD_ALL_LOG_IGNORE("Failed to umount remote");
}

}
