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

#include "../metadata-server/Interface.h"
#include "../metadata-server/Protocol.h"

#include <vector>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>

#include <boost/serialization/binary_object.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

#include <capnp/message.h>
#include <capnp/serialize.h>

#if 0
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#endif

#include <youtils/Logging.h>
#include <youtils/System.h>
#include <gtest/gtest.h>
#include <youtils/wall_timer.h>

namespace volumedrivertest
{

namespace ba = boost::archive;
namespace bio = boost::iostreams;
namespace mds = metadata_server;
namespace mdsproto = metadata_server_protocol;
namespace yt = youtils;

class MetaDataServerProtocolTest
    : public testing::Test
{
    DECLARE_LOGGER("MetaDataServerProtocolTest");

protected:
    MetaDataServerProtocolTest()
        : buf_size_(yt::System::get_env_with_default<size_t>("MDS_SHMEM_SIZE",
                                                             16ULL << 10))
        , val_size_(yt::System::get_env_with_default<size_t>("MDS_VALUE_SIZE",
                                                             4096))
        , nvals_(yt::System::get_env_with_default<size_t>("MDS_MULTIGET_SIZE",
                                                          1))
        , count_(yt::System::get_env_with_default<size_t>("MDS_MULTIGET_COUNT",
                                                          100000))
    {}

    template<typename T,
             typename IArchive = ba::binary_iarchive,
             typename OArchive = ba::binary_oarchive>
    double
    test_serialization_performance(const T& t)
    {
        using Device = bio::basic_array<char>;

        std::vector<char> buf(buf_size_);
        yt::wall_timer w;

        for (size_t i = 0; i < count_; ++i)
        {
            {
                bio::stream_buffer<Device> streambuf(buf.data(),
                                                     buf.size());
                std::ostream os(&streambuf);
                OArchive oa(os);
                oa << t;
            }

            {
                T u;

                bio::stream_buffer<Device> streambuf(buf.data(),
                                                     buf.size());
                std::istream is(&streambuf);
                IArchive ia(is);
                ia >> u;
            }
        }

        return w.elapsed();
    }

    template<typename T>
    double
    test_boost_serialization_performance(const T& t)
    {
        return test_serialization_performance<T>(t);
    }

#if 0
    template<typename T>
    double
    test_cereal_serialization_performance(const T& t)
    {
        return test_serialization_performance<T,
                                              cereal::BinaryInputArchive,
                                              cereal::BinaryOutputArchive>(t);
    }
#endif

    const size_t buf_size_;
    const size_t val_size_;
    const size_t nvals_;
    const size_t count_;
};

TEST_F(MetaDataServerProtocolTest, drop_request)
{
    using Traits = mdsproto::RequestTraits<mdsproto::RequestHeader::Type::Drop>;

    capnp::MallocMessageBuilder builder;
    auto txroot(builder.initRoot<Traits::Params>());

    const std::string nspace("nspace");
    txroot.setNspace(nspace);

    kj::Array<capnp::word> txdata(capnp::messageToFlatArray(builder));

    std::vector<uint8_t> transfer(txdata.size() * sizeof(capnp::word));

    memcpy(transfer.data(),
           txdata.begin(),
           transfer.size());

    auto rxdata = kj::arrayPtr(reinterpret_cast<const capnp::word*>(transfer.data()),
                               transfer.size());

    capnp::FlatArrayMessageReader reader(rxdata);

    auto rxroot(reader.getRoot<Traits::Params>());
    const std::string s(rxroot.getNspace().begin(),
                        rxroot.getNspace().size());

    EXPECT_EQ(s, nspace);
}

TEST_F(MetaDataServerProtocolTest, list_request)
{
    using Traits = mdsproto::RequestTraits<mdsproto::RequestHeader::Type::List>;

    capnp::MallocMessageBuilder builder;
    auto txroot(builder.initRoot<Traits::Params>());
    (void) txroot;

    kj::Array<capnp::word> txdata(capnp::messageToFlatArray(builder));

    std::vector<uint8_t> transfer(txdata.size() * sizeof(capnp::word));

    memcpy(transfer.data(),
           txdata.begin(),
           transfer.size());

    auto rxdata = kj::arrayPtr(reinterpret_cast<const capnp::word*>(transfer.data()),
                               transfer.size());

    capnp::FlatArrayMessageReader reader(rxdata);

    auto rxroot(reader.getRoot<Traits::Params>());
    (void)rxroot;
}

TEST_F(MetaDataServerProtocolTest, capnp_performance)
{
    using Traits = mdsproto::RequestTraits<mdsproto::RequestHeader::Type::MultiGet>;

    const size_t buf_size = yt::System::get_env_with_default<size_t>("MDS_SHMEM_SIZE",
                                                                     16ULL << 10);
    const size_t val_size = yt::System::get_env_with_default<size_t>("MDS_VALUE_SIZE",
                                                                     4096);
    const size_t nvals = yt::System::get_env_with_default<size_t>("MDS_MULTIGET_SIZE",
                                                                  1);
    const size_t count = yt::System::get_env_with_default<size_t>("MDS_MULTIGET_COUNT",
                                                                  100000);

    std::vector<capnp::word> buf(buf_size / sizeof(capnp::word));
    const std::vector<char> val(val_size, 'v');

    mds::TableInterface::MaybeStrings txvals;
    txvals.reserve(nvals);

    for (size_t i = 0; i < nvals; ++i)
    {
        txvals.emplace_back(std::string(val.data(),
                                        val.size()));
    }

    yt::wall_timer w;

    for (size_t i = 0; i < count; ++i)
    {
        memset(buf.data(), 0x0, buf.size());

        capnp::FlatMessageBuilder builder(kj::arrayPtr(buf.data(),
                                                       buf.size()));

        auto txroot(builder.initRoot<Traits::Results>());


        size_t idx = 0;
        auto l = txroot.initValues(txvals.size());

        for (const auto& v : txvals)
        {
            if (v == boost::none)
            {
                capnp::Data::Reader r(nullptr, 0);
                l.set(idx, r);
            }
            else
            {
                capnp::Data::Reader r(reinterpret_cast<const uint8_t*>(v->c_str()),
                                      v->size());
                l.set(idx, r);
            }

            ++idx;
        }

        auto seg(kj::arrayPtr(const_cast<const capnp::word*>(buf.data()),
                              capnp::computeSerializedSizeInWords(builder)));

        capnp::SegmentArrayMessageReader reader(kj::arrayPtr(&seg, 1));
        auto rxroot(reader.getRoot<Traits::Results>());

        mds::TableInterface::MaybeStrings rxvals;
        rxvals.reserve(txvals.size());

        for (const auto& v : rxroot.getValues())
        {
            rxvals.emplace_back(std::string(reinterpret_cast<const char*>(v.begin()),
                                            v.size()));
        }
    }

    const double t = w.elapsed();

    std::cout << count_ << " serializations and deserializations of multiget(" <<
        nvals_ << ") results of size " << val_size_ << " using a buffer of " <<
        buf_size_ << " bytes took " << t << " seconds -> " <<
        count_ / t << " IOPS" << std::endl;
}

TEST_F(MetaDataServerProtocolTest, boost_performance_string)
{
    const std::vector<char> val(val_size_, 'v');

    mds::TableInterface::MaybeStrings txvals;
    txvals.reserve(nvals_);

    for (size_t i = 0; i < nvals_; ++i)
    {
        txvals.emplace_back(std::string(val.data(),
                                        val.size()));
    }

    const double t = test_boost_serialization_performance(txvals);

    std::cout << count_ << " serializations and deserializations of multiget(" <<
        nvals_ << ") results of size " << val_size_ << " using a buffer of " <<
        buf_size_ << " bytes took " << t << " seconds -> " <<
        count_ / t << " IOPS" << std::endl;
}

}

namespace
{

// using our own `Value' here as the mds::Value has a const size member which I don't
// want to unconstify just yet.
struct Value
{
    Value(void* p,
          size_t s)
        : data(p)
        , size(s)
    {}

    Value()
        : data(nullptr)
        , size(0)
    {}

    void* data;
    size_t size;
};

}

namespace boost
{

namespace serialization
{

template<typename Archive>
void
serialize(Archive& ar,
          Value& val,
          const unsigned /* version */)
{
    ar & boost::serialization::make_binary_object(val.data,
                                                  val.size);
}

}

}

#if 0
namespace cereal
{

template<typename Archive,
         typename T>
void
save(Archive& ar,
     const boost::optional<T>& opt)
{
    if (opt)
    {
        ar(true, *opt);
    }
    else
    {
        ar(false);
    }
}

template<typename Archive,
         typename T>
void
load(Archive& ar,
     boost::optional<T>& opt)
{
    bool b;
    ar(b);

    if (b)
    {
        T val;
        ar(val);
        opt = std::move(val);
    }
    else
    {
        opt = boost::none;
    }
}

template<typename Archive>
void
serialize(Archive& ar,
          Value& val)
{
    ar & binary_data(val.data,
                     val.size);
}

}
#endif

namespace volumedrivertest
{

TEST_F(MetaDataServerProtocolTest, boost_performance_blob)
{
    std::vector<char> val(val_size_, 'v');
    std::vector<boost::optional<Value>> txvals;

    txvals.reserve(nvals_);

    for (size_t i = 0; i < nvals_; ++i)
    {
        txvals.emplace_back(Value(val.data(),
                                  val.size()));
    }

    const double t = test_boost_serialization_performance(txvals);

    std::cout << count_ << " serializations and deserializations of multiget(" <<
        nvals_ << ") results of size " << val_size_ << " using a buffer of " <<
        buf_size_ << " bytes took " << t << " seconds -> " <<
        count_ / t << " IOPS" << std::endl;
}

#if 0
TEST_F(MetaDataServerProtocolTest, cereal_performance_blob)
{
    std::vector<char> val(val_size_, 'v');
    std::vector<boost::optional<Value>> txvals;

    txvals.reserve(nvals_);

    for (size_t i = 0; i < nvals_; ++i)
    {
        txvals.emplace_back(Value(val.data(),
                                  val.size()));
    }

    const double t = test_cereal_serialization_performance(txvals);

    std::cout << count_ << " serializations and deserializations of multiget(" <<
        nvals_ << ") results of size " << val_size_ << " using a buffer of " <<
        buf_size_ << " bytes took " << t << " seconds -> " <<
        count_ / t << " IOPS" << std::endl;
}
#endif

TEST_F(MetaDataServerProtocolTest, memcpy_performance_blob)
{
    ASSERT_GE(buf_size_,
              val_size_ * nvals_);

    const std::vector<char> val(val_size_, 'v');

    yt::wall_timer w;

    std::vector<char> wbuf(buf_size_);
    std::vector<char> rbuf(buf_size_);

    for (size_t i = 0; i < count_; ++i)
    {
        for (size_t j = 0; j < nvals_ * val.size(); j += val.size())
        {
            memcpy(wbuf.data() + j,
                   val.data(),
                   val.size());
        }

        for (size_t j = 0; j < nvals_ * val.size(); j += val.size())
        {
            memcpy(rbuf.data() + j,
                   wbuf.data() + j,
                   val.size());
        }
    }

    const double t = w.elapsed();

    std::cout << count_ << " serializations and deserializations of multiget(" <<
        nvals_ << ") results of size " << val_size_ << " using a buffer of " <<
        buf_size_ << " bytes took " << t << " seconds -> " <<
        count_ / t << " IOPS" << std::endl;
}

}
