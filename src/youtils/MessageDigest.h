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

#ifndef YT_MESSAGE_DIGEST_H_
#define YT_MESSAGE_DIGEST_H_

#include "Serialization.h"

#include <iomanip>
#include <iostream>
#include <vector>
#include <cstring>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/io/ios_state.hpp>

namespace youtilstest
{

class WeedTest;

}

namespace youtils
{

template<typename MessageDigestTraits>
class MessageDigest
{
    friend class youtilstest::WeedTest;

public:
    using Traits = MessageDigestTraits;

    MessageDigest(const uint8_t* input, const uint64_t input_size)
    {
        Traits::digest(input,
                       input_size,
                       digest_);
    }

    explicit MessageDigest(const std::string& str)
    {
        VERIFY(str.length() == 2 * Traits::digest_size);

        for (unsigned i = 0; i < Traits::digest_size; ++i)
        {
            digest_[i] = (get_hex_num_(str[2 * i]) << 4) + get_hex_num_(str[2 * i + 1]);
        }
    }

    explicit MessageDigest(const std::vector<uint8_t>& in)
        : MessageDigest(in.data(),
                        in.size())
    {}

    explicit MessageDigest(std::istream& is)
    {
        typename Traits::Context ctx;
        int ret = Traits::digest_init(ctx);

        if (ret == 0)
        {
            throw fungi::IOException("Digest init failed");
        }

        std::vector<char> buf(stream_chunk_size_);

        while (not is.eof())
        {
            is.read(buf.data(),
                    buf.size());
            if (is.fail() and not is.eof())
            {
                throw fungi::IOException("Failed to read bytes from stream for digest");
            }

            ret = Traits::digest_update(ctx,
                                        buf.data(),
                                        is.gcount());
            if (ret == 0)
            {
                throw fungi::IOException("Digest update failed");
            }
        }

        ret = Traits::digest_final(ctx,
                                   digest_);

        if (ret == 0)
        {
            throw fungi::IOException("Digest final failed");
        }
    }

    MessageDigest()
    {
        // This one leaves digest_ intentionally uninitialized for performance reasons -
        // make sure you know what you're doing when using this constructor.
        // Z42: Any way to avoid this?
    };

    typedef boost::archive::binary_iarchive iarchive_type;
    typedef boost::archive::binary_oarchive oarchive_type;

    MessageDigest(const MessageDigest&) = default;

    MessageDigest&
    operator=(const MessageDigest&) = default;

    bool
    operator==(const MessageDigest& other) const
    {
        return memcmp(digest_.data(), other.digest_.data(), Traits::digest_size) == 0;
    }

    bool
    operator!=(const MessageDigest& inother) const
    {
        return not operator==(inother);
    }

    template <typename char_type,
              typename char_type_traits>
    std::basic_ostream<char_type, char_type_traits>&
    print(std::basic_ostream<char_type, char_type_traits>& out) const
    {
        boost::io::ios_all_saver osg(out);

        for(unsigned i = 0; i < Traits::digest_size; ++i)
        {
            unsigned int bt = digest_[i];
            out << std::hex << std::setfill('0') << std::setw(2) << bt;
        }
        return out;
    }

    friend std::ostream&
    operator<<(std::ostream& out, const MessageDigest& w)
    {
        return w.print(out);
    }

    inline bool
    operator>(const MessageDigest& other) const
    {
        return memcmp(digest_.data(), other.digest_.data(), Traits::digest_size) > 0;
    }

    inline bool
    operator<(const MessageDigest& other) const
    {
        return memcmp(digest_.data(), other.digest_.data(), Traits::digest_size) < 0;
    }

    const uint8_t*
    bytes() const
    {
        return digest_.data();
    }

    uint8_t*
    bytes()
    {
        return digest_.data();
    }

    size_t
    size() const
    {
        return Traits::digest_size;
    }

    static const MessageDigest&
    null()
    {
        static const MessageDigest w(make_null_digest_());
        return w;
    }

private:
    DECLARE_LOGGER("MessageDigest");

    static constexpr size_t stream_chunk_size_ = 4096;
    typename Traits::Digest digest_;

    static MessageDigest
    make_null_digest_()
    {
        MessageDigest w;
        memset(w.digest_.data(),
               0x0,
               Traits::digest_size);
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

        uint64_t* w = reinterpret_cast<uint64_t*>(digest_.data());
        for (size_t i = 0; i < Traits::digest_size; i += sizeof(*w))
        {
            ar & *w;
            ++w;
        }
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned int version) const
    {
        if(version != 0)
        {
            THROW_SERIALIZATION_ERROR(version, 0, 0);
        }

        const uint64_t* w = reinterpret_cast<const uint64_t*>(digest_.data());
        for (size_t i = 0; i < Traits::digest_size; i += sizeof(*w))
        {
            ar & *w;
            ++w;
        }
    }

    static uint8_t
    get_hex_num_(char i)
    {
        switch(i)
        {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return (i - '0');

        case 'a':
        case 'A':
            return 0xa;

        case 'b':
        case 'B':
            return 0xb;

        case 'c':
        case 'C':
            return 0xc;

        case 'd':
        case 'D':
            return 0xd;

        case 'e':
        case 'E':
            return 0xe;

        case 'f':
        case 'F':
            return 0xf;

        default:
            throw fungi::IOException("Not a valid hexadecimal character");
        }
    }
};

template<typename Traits>
std::ostream&
operator<<(std::ostream& os,
           const MessageDigest<Traits>& w)
{
    return w.print(os);
}

}

#endif // !YT_MESSAGE_DIGEST_H_
