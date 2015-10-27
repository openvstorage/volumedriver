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

#ifndef VFS_VOLUME_ENTRY_H_
#define VFS_VOLUME_ENTRY_H_

#include <memory>

#include <sys/types.h>

#include <boost/shared_ptr.hpp>

#include <youtils/Assert.h>
#include <youtils/Logging.h>
#include <youtils/Serialization.h>
#include <youtils/UUID.h>

#include <volumedriver/Api.h> // volumedriver/Types.h instead?

namespace volumedriverfs
{

class VolumeEntry
{
public:
    VolumeEntry(mode_t mode,
                uid_t uid,
                gid_t gid)
        : mode_(mode)
        , uid_(uid)
        , gid_(gid)
    {}

    ~VolumeEntry() = default;

    VolumeEntry(const VolumeEntry& other) = default;

    VolumeEntry&
    operator=(const VolumeEntry& other) = default;

    volumedriver::VolumeId
    id() const
    {
        return volumedriver::VolumeId(uuid_.str());
    }

    mode_t
    mode() const
    {
        return mode_;
    }

    void
    mode(mode_t m)
    {
        mode_ = S_IFREG bitor m;
    }

    uid_t
    uid() const
    {
        return uid_;
    }

    void
    uid(uid_t uid)
    {
        uid_ = uid;
    }

    gid_t
    gid() const
    {
        return gid_;
    }

    void
    gid(gid_t gid)
    {
        gid_ = gid;
    }

private:
    DECLARE_LOGGER("VolumeEntry");

    const youtils::UUID uuid_;
    mode_t mode_;
    uid_t uid_;
    gid_t gid_;

    friend class boost::serialization::access;

    template<typename Archive>
    void
    serialize(Archive& ar, const unsigned int version)
    {
        CHECK_VERSION(version, 2);

        ar & boost::serialization::make_nvp("uuid",
                                            const_cast<youtils::UUID&>(uuid_));
        ar & boost::serialization::make_nvp("mode",
                                            const_cast<mode_t&>(mode_));
        ar & boost::serialization::make_nvp("uid",
                                            const_cast<uid_t&>(uid_));
        ar & boost::serialization::make_nvp("gid",
                                            const_cast<gid_t&>(gid_));
    }
};

// works better with boost::serialization
typedef boost::shared_ptr<VolumeEntry> VolumeEntryPtr;

}

BOOST_CLASS_VERSION(volumedriverfs::VolumeEntry, 2);

namespace boost
{

namespace serialization
{

template<typename Archive>
inline void
load_construct_data(Archive&,
                    volumedriverfs::VolumeEntry* entry,
                    const unsigned int /* version */)
{
    new(entry) volumedriverfs::VolumeEntry(0, 0, 0);
}

}

}

#endif // !VFS_VOLUME_ENTRY_H_
