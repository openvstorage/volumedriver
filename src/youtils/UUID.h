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

#ifndef _UUID_H_
#define _UUID_H_

#include <string>
#include "Logging.h"
#include "Serialization.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_serialize.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "SpinLock.h"

namespace youtils
{

class UUID
{
public:
    friend class boost::serialization::access;

    UUID();

    explicit UUID(const std::string&);

    explicit UUID(const char* in);

    UUID(const UUID&);

    bool
    operator==(const UUID& other) const;

    UUID&
    operator=(const UUID& other);

    bool
    operator!=(const UUID& other) const;

    bool
    operator<(const UUID& other) const;

    bool
    operator>(const UUID& other) const;

    bool
    isNull() const;

    static bool
    isUUIDString(const std::string& );

    static bool
    isUUIDString(const char* in);

    static inline uint64_t
    getUUIDStringSize()
    {
        return 36;
    }

    std::string
    str() const;

    DECLARE_LOGGER("UUID");

    static const UUID& NullUUID()
    {
        static const UUID nuuid(boost::uuids::nil_uuid());
        return nuuid;
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        if(version == 0)
        {
            ar & boost::serialization::make_nvp("uuid",
                                                uuid_);
        }
        else
        {
            THROW_SERIALIZATION_ERROR(version, 0, 0);
        }
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned int version) const
    {
        if(version == 0)
        {
            ar & boost::serialization::make_nvp("uuid",
                                                uuid_);
        }
        else
        {
            THROW_SERIALIZATION_ERROR(version, 0, 0);
        }
    }

    const uint8_t*
    data() const
    {
        return uuid_.data;
    }

    static size_t
    size()
    {
        return sizeof(uuid_);
    }

    operator const boost::uuids::uuid&() const
    {
        return uuid_;
    }

private:
    static fungi::SpinLock random_generator_lock_;
    static boost::uuids::random_generator random_generator_;

    boost::uuids::uuid uuid_;
    UUID(bool) = delete;

    explicit
    UUID(const boost::uuids::uuid& in_uuid)
    {
        uuid_ = in_uuid;
    }

    static inline bool
    hasonlyhexdigits(const std::string& str,
                     size_t i1,
                     size_t i2);

    static inline bool
    hasonlyhexdigits(const char* in,
                     size_t i1,
                     size_t i2);

    friend std::ostream&
    operator<<(std::ostream& ostr, const UUID& uuid);
};

}

BOOST_CLASS_VERSION(::youtils::UUID,0);

#endif // _UUID_H_

// Local Variables: **
// mode: c++ **
// End: **
