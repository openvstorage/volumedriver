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

#include "DirectoryEntry.h"

#include <type_traits>

#include <sys/stat.h>

#include <youtils/UUID.h>


namespace volumedriverfs
{

namespace yt = youtils;

#define LOCK()                                  \
    boost::lock_guard<decltype(lock_)> lg__(lock_)

DirectoryEntry::DirectoryEntry(const ObjectId& oid,
                               Type type,
                               Inode inode,
                               Permissions permissions,
                               UserId user_id,
                               GroupId group_id)
    : oid_(oid)
    , type_(type)
    , inode_(inode)
    , permissions_(permissions)
    , user_id_(user_id)
    , group_id_(group_id)
{
    LOG_TRACE(oid_ << ": permissions " << permissions_ <<
              ", user_id " << user_id_ << ", group_id " << group_id_);

    auto ret = ::gettimeofday(&ctime_, nullptr);
    if (ret < 0)
    {
        ret = errno;
        LOG_ERROR("gettimeofday failed: " << strerror(ret));
        throw fungi::IOException("gettimeofday failed",
                                 "gettimeofday",
                                 ret);
    }

    atime_ = mtime_ = ctime_;
}

DirectoryEntry::DirectoryEntry(Type type,
                               Inode inode,
                               Permissions permissions,
                               UserId user_id,
                               GroupId group_id)
    : DirectoryEntry(ObjectId(yt::UUID().str()),
                     type,
                     inode,
                     permissions,
                     user_id,
                     group_id)
{}

#define IN(x)                                                 \
    x(other.x)

DirectoryEntry::DirectoryEntry(const DirectoryEntry& other)
    : IN(oid_)
    , IN(type_)
    , IN(inode_)
    , IN(permissions_)
    , IN(user_id_)
    , IN(group_id_)
    , IN(atime_)
    , IN(ctime_)
    , IN(mtime_)
{}

#undef IN

DirectoryEntry&
DirectoryEntry::operator=(const DirectoryEntry& other)
{
    if (this != &other)
    {
#define CP(x)                                   \
        const_cast<std::add_lvalue_reference<std::remove_const<decltype(x)>::type>::type>(x) = other.x

        CP(oid_);
        CP(type_);
        CP(inode_);
        CP(permissions_);
        CP(user_id_);
        CP(group_id_);
        CP(atime_);
        CP(ctime_);
        CP(mtime_);

#undef CP
    }

    return *this;
}

bool
DirectoryEntry::operator==(const DirectoryEntry& other) const
{
    if (this == &other)
    {
        return true;
    }
    else
    {

#define EQ(x)                                   \
    (x == other.x)

        LOCK();

        return
            EQ(oid_) and
            EQ(type_) and
            EQ(inode_) and
            EQ(permissions_) and
            EQ(user_id_) and
            EQ(group_id_) and
            EQ(atime_.tv_sec) and
            EQ(atime_.tv_usec) and
            EQ(ctime_.tv_sec) and
            EQ(ctime_.tv_usec) and
            EQ(mtime_.tv_sec) and
            EQ(mtime_.tv_usec);

#undef EQ

    }
}

void
DirectoryEntry::permissions(Permissions p)
{
    LOCK();

    LOG_TRACE(oid_ << ": permissions " << p);
    permissions_ = p;
}

}
