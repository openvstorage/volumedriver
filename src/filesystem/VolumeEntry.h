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
