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

#ifndef FILE_SYSTEM_TEST_REMOTE_MOUNT_H_
#define FILE_SYSTEM_TEST_REMOTE_MOUNT_H_

#include <youtils/Logging.h>

namespace volumedriverfstest
{

class FileSystemTestBase;

class RemoteMount
{
public:
    ~RemoteMount();

    RemoteMount(const RemoteMount&) = delete;

    RemoteMount&
    operator=(const RemoteMount&) = delete;

    RemoteMount(RemoteMount&&);

    RemoteMount&
    operator=(RemoteMount&&);

    void
    mount();

    void
    umount(bool ignore_errors = false);

private:
    friend class FileSystemTestBase;

    DECLARE_LOGGER("RemoteMount");

    FileSystemTestBase* base_;
    bool mounted_ = false;

    explicit RemoteMount(FileSystemTestBase&);
};

}

#endif // !FILE_SYSTEM_TEST_REMOTE_MOUNT_H_
