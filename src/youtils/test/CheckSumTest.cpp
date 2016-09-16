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

#include "../CheckSum.h"
#include <gtest/gtest.h>
#include "../wall_timer.h"
#include "../System.h"

#include <arpa/inet.h>
#include <boost/crc.hpp>

namespace youtilstest
{

namespace yt = youtils;

template<typename>
struct CheckSumTraits
{};

class SWCheckSum;
class BuchlaCheckSum;

class CheckSumTest
    : public testing::Test
{
public:
    static uint32_t
    crc32c_sw(uint32_t crc,
              const void* data,
              size_t size)
    {
        return yt::CheckSum::crc32c_sw_(crc,
                                        data,
                                        size);
    }

protected:
    DECLARE_LOGGER("CheckSumTest");

    template<typename T,
             typename Traits = CheckSumTraits<T>>
    void
    perftest(T& cs)
    {
        yt::wall_timer w;

        uint64_t count = 1ULL << 20;
        count = yt::System::get_env_with_default("ITERATIONS",
                                                 count);

        for (uint64_t i = 0; i < count; ++i)
        {
            Traits::update(cs,
                           &i,
                           sizeof(i));
        }

        double t = w.elapsed();

        LOG_INFO(count << " iterations took " << t << " seconds => " <<
                 (count / t) << " OPS");
    }

    template<typename T,
             typename Traits = CheckSumTraits<T>>
    void
    check_(const void* data,
           size_t size,
           uint32_t crc32c)
    {
        T cs;

        Traits::update(cs,
                       data,
                       size);

        const uint32_t val = Traits::crc32c(cs);

        EXPECT_EQ(crc32c,
                  val) <<
            Traits::name() << ": " <<
            std::hex << std::setw(8) << std::setfill('0') <<
            "expected: " << crc32c << ", got: " << val;

        EXPECT_EQ(val,
                  Traits::value(T(crc32c)));
    }

    void
    check_known_crc(const void* data,
                    size_t size,
                    uint32_t crc32c)
    {
        check_<yt::CheckSum>(data,
                             size,
                             crc32c);

        check_<SWCheckSum>(data,
                           size,
                           crc32c);

        check_<BuchlaCheckSum>(data,
                               size,
                               crc32c);
    }

    // RFC 3720 uses BE byte order.
    void
    check_rfc(const std::vector<uint8_t>& buf,
              uint32_t crc32c)
    {
        check_known_crc(buf.data(),
                        buf.size(),
                        ntohl(crc32c));
    }
};

// The previous implementation inherited from an old backend - it lacks the final
// XOR with 0xFFFFFFFF
struct BuchlaCheckSum
{
    typedef boost::crc_optimal<32, // Bits
                               0x1EDC6F41, // TruncPoly
                               0xFFFFFFFF, // InitialRemainder
                               0x0, // Final XOR Value
                               true, // ReflectIn
                               true> // ReflectRem
    checksum_type;

    typedef uint32_t value_type;
    typedef boost::detail::reflector<checksum_type::bit_count> reflector_type;

    BuchlaCheckSum()
        : checksum_(checksum_type::initial_remainder)
    {}

    explicit BuchlaCheckSum(value_type val)
        : checksum_(val)
    {}

    void
    update(const void* buf, size_t size)
    {
        checksum_type cs(reflector_type::reflect(checksum_));
        cs.process_bytes(buf, size);
        checksum_ = cs.checksum();
    }

    value_type
    value() const
    {
        return checksum_;
    }

    value_type checksum_;
};

template<>
struct CheckSumTraits<BuchlaCheckSum>
{
    static const char*
    name()
    {
        return "BuchlaCheckSum";
    }

    static uint32_t
    crc32c(const BuchlaCheckSum& cs)
    {
        return ~cs.value();
    }

    static uint32_t
    value(const BuchlaCheckSum& cs)
    {
        return cs.value();
    }

    static void
    update(BuchlaCheckSum& cs,
           const void* data,
           size_t size)
    {
        cs.update(data,
                  size);
    }
};

struct SWCheckSum
{
    static constexpr uint32_t initial_value_ = ~0U;
    uint32_t crc_;

    SWCheckSum()
        : crc_(initial_value_)
    {}

    SWCheckSum(uint32_t crc)
        : crc_(~crc)
    {}

    void
    update(const void* data,
           size_t size)
    {
        crc_ = CheckSumTest::crc32c_sw(crc_,
                                       data,
                                       size);
    }

    uint32_t
    value() const
    {
        return ~crc_;
    }
};

template<>
struct CheckSumTraits<SWCheckSum>
{
    static const char*
    name()
    {
        return "SWCheckSum";
    }

    static uint32_t
    crc32c(const SWCheckSum& cs)
    {
        return cs.value();
    }

    static uint32_t
    value(const SWCheckSum& cs)
    {
        return cs.value();
    }

    static void
    update(SWCheckSum& cs,
           const void* data,
           size_t size)
    {
        cs.update(data,
                  size);
    }
};

template<>
struct CheckSumTraits<yt::CheckSum>
{
    static const char*
    name()
    {
        return "CheckSum";
    }

    static uint32_t
    crc32c(const yt::CheckSum& cs)
    {
        return cs.getValue();
    }

    static uint32_t
    value(const yt::CheckSum& cs)
    {
        return cs.getValue();
    }

    static void
    update(yt::CheckSum& cs,
           const void* data,
           size_t size)
    {
        cs.update(data,
                  size);
    }
};

TEST_F(CheckSumTest, buchla_vs_crc32c)
{
    uint64_t count = 1ULL << 20;
    count = yt::System::get_env_with_default("ITERATIONS",
                                             count);

    {
        BuchlaCheckSum buchla;
        yt::CheckSum cs;

        EXPECT_EQ(cs.getValue(),
                  ~buchla.value());
    }

    {
        const uint32_t i = 1;
        BuchlaCheckSum buchla(i);
        yt::CheckSum cs(i);

        EXPECT_EQ(cs.getValue(),
                  buchla.value());
    }

    for (uint64_t i = 0; i < count; ++i)
    {
        BuchlaCheckSum buchla;
        buchla.update(&i, sizeof(i));

        yt::CheckSum cs;
        cs.update(&i, sizeof(i));

        EXPECT_EQ(cs.getValue(),
                  ~buchla.value());
    }

    {
        BuchlaCheckSum buchla;
        yt::CheckSum cs;

        for (uint64_t i = 0; i < count; ++i)
        {
            cs.update(&i, sizeof(i));
            buchla.update(&i, sizeof(i));
        }

        EXPECT_EQ(cs.getValue(),
                  ~buchla.value());
    }
}

TEST_F(CheckSumTest, boost_buchla_perf)
{
    BuchlaCheckSum cs;
    perftest(cs);
}

TEST_F(CheckSumTest, sw_perf)
{
    SWCheckSum cs;
    perftest(cs);
}

TEST_F(CheckSumTest, perf)
{
    yt::CheckSum cs;
    perftest(cs);
}

TEST_F(CheckSumTest, known_values)
{
    const std::string numbers("1234567890");
    const std::string phrase("The quick brown fox jumps over the lazy dog");

    check_known_crc(numbers.data(),
                    9,
                    0xe3069283);

    check_known_crc(numbers.data() + 1,
                    8,
                    0xbfe92a83);

    check_known_crc(numbers.data(),
                    numbers.size(),
                    0xf3dbd4fe);

    check_known_crc(phrase.data(),
                    phrase.size(),
                    0x22620404);
}

TEST_F(CheckSumTest, zero)
{
    EXPECT_EQ(0U,
              yt::CheckSum().getValue());

    EXPECT_EQ(0U,
              yt::CheckSum(0).getValue());

    EXPECT_EQ(0UL,
              SWCheckSum().value());

    EXPECT_EQ(0U,
              SWCheckSum(0).value());

    EXPECT_EQ(~0U,
              BuchlaCheckSum().value());

    EXPECT_EQ(0U,
              BuchlaCheckSum(0).value());
}

TEST_F(CheckSumTest, rfc3720_b4)
{
    const std::vector<uint8_t> zeroes(32, 0);
    check_rfc(zeroes,
              0xaa36918a);

    const std::vector<uint8_t> ones(32, 0xff);
    check_rfc(ones,
              0x43aba862);

    std::vector<uint8_t> incr(32);
    for (size_t i = 0; i < incr.size(); ++i)
    {
        incr[i] = i;
    }

    check_rfc(incr,
              0x4e79dd46);

    std::vector<uint8_t> decr(incr.size());
    std::reverse_copy(incr.begin(),
                      incr.end(),
                      decr.begin());

    check_rfc(decr,
              0x5cdb3f11);

    const std::vector<uint8_t> write_10 {
        0x01, 0xc0, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x14, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x04, 0x00,
        0x00, 0x00, 0x00, 0x14,
        0x00, 0x00, 0x00, 0x18,
        0x28, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };

    check_rfc(write_10,
              0x563a96d9);
}

}
