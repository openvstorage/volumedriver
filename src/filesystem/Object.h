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
        return os << "File";
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
