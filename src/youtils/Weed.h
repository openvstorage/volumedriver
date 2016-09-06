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

namespace youtilstest
{
class WeedTest;
}

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
    friend class youtilstest::WeedTest;
    friend class volumedrivertest::KaKPerformanceTest;
    friend class volumedrivertest::ClusterCacheMapTest;

public:
    Weed(const byte* input, const uint64_t input_size);

    explicit Weed(const std::string& str);

    explicit Weed(const std::vector<uint8_t>& in);

    explicit Weed(std::istream&);

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

    static constexpr size_t stream_chunk_size_ = 4096;
};

static_assert(sizeof(Weed) == Weed::weed_size,
              "sizeof(Weed) oddity encountered");
}

BOOST_CLASS_VERSION(youtils::Weed, 0);

#endif // _WEED_H_

// Local Variables: **
// mode: c++ **
// End: **
