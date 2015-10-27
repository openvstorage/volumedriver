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

#ifndef VFS_DIRECTORY_ENTRY_H_
#define VFS_DIRECTORY_ENTRY_H_

#include "Object.h"
#include "Permissions.h"

#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/serialization/serialization.hpp>

#include <youtils/Assert.h>
#include <youtils/Logging.h>
#include <youtils/OurStrongTypedef.h>
#include <youtils/Serialization.h>

// There's not much point in doing arithmetic operations on these?
OUR_STRONG_NON_ARITHMETIC_TYPEDEF(uid_t, UserId, volumedriverfs);
OUR_STRONG_NON_ARITHMETIC_TYPEDEF(gid_t, GroupId, volumedriverfs);
OUR_STRONG_NON_ARITHMETIC_TYPEDEF(ino_t, Inode, volumedriverfs)

namespace volumedriverfs
{

class DirectoryEntry
{

#define LOCK()                                  \
    boost::lock_guard<decltype(lock_)> lg__(lock_)

public:
    // Leverage polymorphism instead of open-coded switch statements
    // (but OTOH boost::serialization does not play too nicely with it)?
    enum class Type
    {
        Directory,
        Volume,
        File,
    };

    DirectoryEntry(Type type,
                   Inode inode,
                   Permissions permissions,
                   UserId user_id,
                   GroupId group_id);

    DirectoryEntry(const ObjectId& oid,
                   Type type,
                   Inode inode,
                   Permissions permissions,
                   UserId user_id,
                   GroupId group_id);

    ~DirectoryEntry() = default;

    DirectoryEntry(const DirectoryEntry&);

    DirectoryEntry&
    operator=(const DirectoryEntry&);

    bool
    operator==(const DirectoryEntry&) const;

    bool
    operator!=(const DirectoryEntry& other) const
    {
        return not operator==(other);
    }

    const ObjectId&
    object_id() const
    {
        return oid_;
    }

    Type
    type() const
    {
        return type_;
    }

    Inode
    inode() const
    {
        return inode_;
    }

    Permissions
    permissions() const
    {
        return permissions_;
    }

    void
    permissions(Permissions m);

    UserId
    user_id() const
    {
        LOCK();
        return user_id_;
    }

    void
    user_id(UserId u)
    {
        LOCK();
        LOG_TRACE(oid_ << ": " << u);
        user_id_ = u;
    }

    GroupId
    group_id() const
    {
        LOCK();
        return group_id_;
    }

    void
    group_id(GroupId g)
    {
        LOCK();
        LOG_TRACE(oid_ << ": " << g);
        group_id_ = g;
    }

    timeval
    atime() const
    {
        LOCK();
        return atime_;
    }

    void
    atime(const timeval& tv)
    {
        LOCK();
        atime_ = tv;
    }

    timeval
    ctime() const
    {
        LOCK();
        return ctime_;
    }

    timeval
    mtime() const
    {
        LOCK();
        return mtime_;
    }

    void
    mtime(const timeval& tv)
    {
        LOCK();
        mtime_ = tv;
    }

private:
    DECLARE_LOGGER("DirectoryEntry");

    // XXX: use std::atomics instead?
    mutable boost::mutex lock_;
    const ObjectId oid_;
    const Type type_;
    const Inode inode_;
    Permissions permissions_;
    UserId user_id_;
    GroupId group_id_;
    timeval atime_;
    timeval ctime_;
    timeval mtime_;

    friend class boost::serialization::access;

    template<class Archive>
    void
    serialize(Archive& ar, const unsigned version)
    {
        CHECK_VERSION(version, 2);

        ar & boost::serialization::make_nvp("type",
                                            const_cast<Type&>(type_));
        ar & boost::serialization::make_nvp("inode",
                                            const_cast<Inode&>(inode_));
        ar & boost::serialization::make_nvp("oid",
                                            const_cast<ObjectId&>(oid_));
        ar & boost::serialization::make_nvp("permissions",
                                            const_cast<Permissions&>(permissions_));
        ar & boost::serialization::make_nvp("user_id",
                                            const_cast<UserId&>(user_id_));
        ar & boost::serialization::make_nvp("group_id",
                                            const_cast<GroupId&>(group_id_));
        ar & boost::serialization::make_nvp("atime",
                                            const_cast<timeval&>(atime_));
        ar & boost::serialization::make_nvp("ctime",
                                            const_cast<timeval&>(ctime_));
        ar & boost::serialization::make_nvp("mtime",
                                            const_cast<timeval&>(mtime_));
    }

#undef LOCK
};

typedef boost::shared_ptr<DirectoryEntry> DirectoryEntryPtr;

}

BOOST_CLASS_VERSION(volumedriverfs::DirectoryEntry, 2);

namespace boost
{

namespace serialization
{

template<typename Archive>
void
serialize(Archive& ar,
          timeval& tv,
          const unsigned /* version */)
{
    ar & boost::serialization::make_nvp("seconds",
                                        tv.tv_sec);
    ar & boost::serialization::make_nvp("microseconds",
                                        tv.tv_usec);
}

template<typename Archive>
inline void
load_construct_data(Archive& /* ar */,
                    volumedriverfs::DirectoryEntry* dentry,
                    const unsigned int /* version */)
{
    using namespace volumedriverfs;

    new(dentry) DirectoryEntry(DirectoryEntry::Type::Directory,
                               Inode(0),
                               Permissions(0),
                               UserId(0),
                               GroupId(0));
}

}

}

#endif // ! VFS_DIRECTORY_ENTRY_H_
