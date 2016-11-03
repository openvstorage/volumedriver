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

#ifndef VFS_OBJECT_H_
#define VFS_OBJECT_H_

#include <boost/optional.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/serialization.hpp>

#include <youtils/Assert.h>
#include <youtils/StrongTypedString.h>

#include <volumedriver/Types.h>

STRONG_TYPED_STRING(volumedriverfs, ObjectId);

namespace volumedriverfs
{

// XXX: find a better place for that one:
typedef boost::optional<volumedriver::SnapshotName> MaybeSnapshotName;

enum class ObjectType
{
    File,
    Volume,
    Template,
};

inline std::ostream&
operator<<(std::ostream& os,
           ObjectType tp)
{
    switch (tp)
    {
    case ObjectType::File:
        return os << "File";
    case ObjectType::Volume:
        return os << "Volume";
    case ObjectType::Template:
        return os << "Template";
    }
    UNREACHABLE;
}

// XXX:
// - Use polymorphism instead?
// - There is some overlap with DirectoryEntry as that also has a type (slightly different
//   from ObjectType) and carries the ObjectId, which stems from the fact that the
//   FileSystem metadata in arakoon and the object registrations are related / have some
//   redundancies. See if that can be unified.
struct Object
{
    Object(ObjectType tp,
           const ObjectId& i)
        : type(tp)
        , id(i)
    {}

    ~Object() = default;

    Object(const Object&) = default;

    Object&
    operator=(const Object&) = default;

    const ObjectType type;
    const ObjectId id;
};

inline std::ostream&
operator<<(std::ostream& os,
           const Object& obj)
{
    os << obj.type << "-" << obj.id;
    return os;
}

}

#endif // !VFS_OBJECT_H_
