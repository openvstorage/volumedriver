// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _WEED_H_
#define _WEED_H_

#include "Serialization.h"

#include <iomanip>
#include <vector>
#include <cstring>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/io/ios_state.hpp>

// #define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <openssl/md5.h>

namespace volumedrivertest
{
class KaKPerformanceTest;
class ClusterCacheMapTest;

}
// Find a better place for this??
typedef unsigned char byte;
namespace youtils
{

class Weed
{
    friend class volumedrivertest::KaKPerformanceTest;
    friend class volumedrivertest::ClusterCacheMapTest;

public:
    Weed(const byte* input, const uint64_t input_size);

    explicit Weed(const std::string& str);

    explicit Weed(const std::vector<uint8_t>& in);

    Weed()
    {
        // This one leaves weed_ intentionally uninitialized for performance reasons -
        // make sure you know what you're doing when using this constructor.
        // Z42: Any way to avoid this?
    };

    typedef boost::archive::binary_iarchive iarchive_type;
    typedef boost::archive::binary_oarchive oarchive_type;

    Weed(const Weed&) = default;

    Weed&
    operator=(const Weed&) = default;

    static const uint32_t weed_size = MD5_DIGEST_LENGTH;

    bool
    check(const byte* input, const uint64_t input_size) const;

    bool
    operator==(const Weed& inother) const;

    bool
    operator!=(const Weed& inother) const
    {
        return not operator==(inother);
    }

    template <typename char_type,
              typename char_type_traits>
    std::basic_ostream<char_type, char_type_traits>&
    print(std::basic_ostream<char_type, char_type_traits>& out) const
    {
        boost::io::ios_all_saver osg(out);

        for(unsigned i = 0; i < weed_size; ++i)
        {
            unsigned int bt = weed_[i];
            out << std::hex << std::setfill('0') << std::setw(2) << bt;
        }
        return out;
    }

    friend std::ostream&
    operator<<(std::ostream& out, const Weed& w)
    {
        return w.print(out);
    }

    inline bool
    operator>(const Weed& w2) const
    {
        static_assert(weed_size == 2 * sizeof(uint64_t), "Weed comparison needs review");
        const uint64_t* w_p1 = reinterpret_cast<const uint64_t*>(weed_);
        const uint64_t* w_p2 = reinterpret_cast<const uint64_t*>(w2.weed_);
        if(*w_p1 > *w_p2)
        {
            return true;
        }
        else if( *w_p1 < *w_p2)
        {
            return false;
        }
        else
        {
            return *(++w_p1) > *(++w_p2);
        }
    }

    inline bool
    operator<(const Weed& other) const
    {
        return *this != other and not (*this > other);
    }

    const uint8_t*
    bytes() const
    {
        return weed_;
    }

    uint8_t*
    bytes()
    {
        return weed_;
    }

    size_t
    size() const
    {
        return weed_size;
    }

    static const Weed&
    null()
    {
        static const Weed w(make_null_weed_());
        return w;
    }

private:
    DECLARE_LOGGER("Weed");

    uint8_t weed_[weed_size];

    static Weed
    make_null_weed_()
    {
        Weed w;
        memset(w.weed_,
               0x0,
               weed_size);
        return w;
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    friend class boost::serialization::access;

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        if(version != 0)
        {
            THROW_SERIALIZATION_ERROR(version, 0, 0);
        }
        uint64_t* w_p1 = reinterpret_cast<uint64_t*>(weed_);
        ar & *w_p1;
        ar & *(++w_p1);
    }

    template<class Archive>
    void save(Archive& ar, const unsigned int version) const
    {
        if(version != 0)
        {
            THROW_SERIALIZATION_ERROR(version, 0, 0);
        }
        const uint64_t* w_p1 = reinterpret_cast<const uint64_t*>(weed_);
        ar & *w_p1;
        ar & *(++w_p1);
    }
};

static_assert(sizeof(Weed) == Weed::weed_size,
              "sizeof(Weed) oddity encountered");
}

BOOST_CLASS_VERSION(youtils::Weed, 0);

#endif // _WEED_H_

// Local Variables: **
// mode: c++ **
// End: **
