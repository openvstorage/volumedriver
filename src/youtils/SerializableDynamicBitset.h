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

#ifndef SERIALIZABLE_DYNAMIC_BITSET
#define SERIALIZABLE_DYNAMIC_BITSET

#define BOOST_DYNAMIC_BITSET_DONT_USE_FRIENDS
#include <boost/dynamic_bitset.hpp>


// #undef BOOST_DYNAMIC_BITSET_PRIVATE
// #define BOOST_DYNAMIC_BITSET_PRIVATE public
// // #define BOOST_DYNAMIC_BITSET_PRIVATE public
// #undef BOOST_DYNAMIC_BITSET_PRIVATE

#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
#include "Serialization.h"
#include "Logging.h"
#include "Assert.h"
namespace youtils
{
class SerializableDynamicBitset : public boost::dynamic_bitset<>
{
    typedef boost::dynamic_bitset<> bitset;

    static_assert(sizeof(bitset::block_type) == 8,
                  "strange size for block type");
    static_assert(bitset::bits_per_block == 64,
                  "strange size for bits per block");

public:
    SerializableDynamicBitset()
        :bitset()
    {}

    SerializableDynamicBitset(bitset::size_type num_bits)
        : bitset(num_bits)
    {}


    SerializableDynamicBitset(const SerializableDynamicBitset& ) = delete;


    SerializableDynamicBitset&
    operator=(const SerializableDynamicBitset&) = delete;


    bool
    operator[](bitset::size_type a) const
    {
        return bitset::operator[](a);
    }

    bitset::reference
    operator[](bitset::size_type a)
    {
        growAsNeeded(a);
        ASSERT(size() > a);
        return bitset::operator[](a);
    }

private:


    friend class boost::serialization::access;
    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        if(version != 1)
        {
            THROW_SERIALIZATION_ERROR(version,1,1);
        }

        ar & m_num_bits;
        ar & m_bits;
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned int version) const
    {
        if(version != 1)
        {
            THROW_SERIALIZATION_ERROR(version,1,1);
        }

        ar & m_num_bits;
        ar & m_bits;
    }

    DECLARE_LOGGER("SerializableDynamicBitset");

    void
    growAsNeeded(bitset::size_type a)
    {
        if (size() <= a)
        {
            if(a > max_size())
            {
                LOG_INFO("Cannot make a bitset larger than " << max_size() << ", requested " << a);
                throw fungi::IOException("Bitset to small");
            }
            size_t cur_size = std::max(static_cast<bitset::size_type>(1), size());
            while(cur_size <= a)
            {
                cur_size *= 2;
            }

            LOG_INFO("Resizing bitset to " << std::min(max_size(), cur_size));

            resize(std::min(max_size(), cur_size));
        }
    }

};

}
BOOST_CLASS_VERSION(youtils::SerializableDynamicBitset, 1);
#endif // SERIALIZABLE_DYNAMIC_BITSET

// Local Variables: **
// End: **
