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

#include "../Logging.h"
#include "../Serialization.h"
#include "../TestBase.h"

#include <memory>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <boost/shared_ptr.hpp>

namespace youtilstest
{

namespace ba = boost::archive;
namespace bs = boost::serialization;

namespace
{

struct WithoutNVP
{
    explicit WithoutNVP(const std::string& s)
        : str(s)
    {}

    std::string str;

    template<class Archive>
    void
    serialize(Archive& ar, const unsigned int /* version */)
    {
        ar & str;
    }
};

struct WithNVP
{
    explicit WithNVP(const std::string& s)
        : str(s)
    {}

    std::string str;

    template<class Archive>
    void
    serialize(Archive& ar, const unsigned int /* version */)
    {
        ar & bs::make_nvp("string", str);
    }
};

struct TwoStrings
{
    TwoStrings(const std::string& o,
               const std::string& t)
        : one(o)
        , two(t)
    {}

    std::string one;
    std::string two;

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<typename Archive>
    void
    load(Archive& ar, const unsigned int /* version */)
    {
        ar & bs::make_nvp("one", one);
        ar & bs::make_nvp("two", two);
    }

    template<typename Archive>
    void
    save(Archive& ar, const unsigned int /* version */) const
    {
        ar & bs::make_nvp("two", two);
        ar & bs::make_nvp("one", one);
    }
};

}

class SerializationTest
    : public TestBase
{
protected:
    template<typename IArchive, typename OArchive>
    void
    test_roundtrip()
    {
        const std::string str("foo");

        WithNVP with(str);
        EXPECT_EQ(str, with.str);

        std::stringstream ss;
        {
            OArchive oa(ss);
            oa << with;
        }

        WithoutNVP without("bar");
        {
            IArchive ia(ss);
            ia >> without;
        }

        EXPECT_EQ(str, without.str);

        {
            std::stringstream tt;
            OArchive oa(tt);
            oa << without;

            IArchive ia(tt);
            WithNVP with("baz");
            ia >> with;

            EXPECT_EQ(str, with.str);
        }
    }

    template<typename IArchive, typename OArchive>
    void
    test_order()
    {
        const TwoStrings t("1", "2");

        std::stringstream ss;
        OArchive oa(ss);
        oa << t;

        TwoStrings u("eins", "zwei");
        IArchive ia(ss);
        ia >> u;

        EXPECT_EQ(t.one, u.one);
        EXPECT_EQ(t.two, u.two);
    }

    DECLARE_LOGGER("SerializationTest");
};

TEST_F(SerializationTest, with_or_without_nvp_binary)
{
    test_roundtrip<ba::binary_iarchive,
                   ba::binary_oarchive>();
}

TEST_F(SerializationTest, with_or_without_nvp_text)
{
    test_roundtrip<ba::text_iarchive,
                   ba::text_oarchive>();
}

// the following two show that order does matter
TEST_F(SerializationTest, DISABLED_split_members_order_text)
{
    test_order<ba::text_iarchive,
               ba::text_oarchive>();
}

TEST_F(SerializationTest, DISABLED_split_members_order_binary)
{
    test_order<ba::binary_iarchive,
               ba::binary_oarchive>();
}

struct Base
{
    Base()
        : bname_("uninitialized")
    {
        LOG_INFO("default constructor: " << bname_);
    }

    explicit Base(const std::string& s)
        : bname_(s)
    {
        LOG_INFO("constructor: " << bname_);
    }

    virtual ~Base() = default;

    const std::string&
    bname() const
    {
        return bname_;
    }

    template<class Archive>
    void serialize(Archive& ar, const unsigned int /* version */)
    {
        LOG_INFO("base");
        ar & BOOST_SERIALIZATION_NVP(bname_);
    }

    DECLARE_LOGGER("Base");

    std::string bname_;
};

struct Derived
    : public Base
{
    Derived()
        : Base("uninitialized-base")
        , dname_("uninitialized")
    {
        LOG_INFO("default constructor: " << dname_);
    }

    explicit Derived(const std::string& s)
        : Base(s + std::string("-base"))
        , dname_(s)
    {
        LOG_INFO("constructor: " << dname_);
    }

    virtual ~Derived() = default;

    const std::string&
    dname() const
    {
        return dname_;
    }

    template<class Archive>
    void serialize(Archive& ar, const unsigned int /* version */)
    {
        LOG_INFO("derived");
        ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Base);
        ar & BOOST_SERIALIZATION_NVP(dname_);
    }

    DECLARE_LOGGER("Derived");

    std::string dname_;
};

}

BOOST_CLASS_EXPORT(youtilstest::Base);
BOOST_CLASS_VERSION(youtilstest::Base, 1);

BOOST_CLASS_EXPORT(youtilstest::Derived);
BOOST_CLASS_VERSION(youtilstest::Derived, 1);

namespace youtilstest
{

TEST_F(SerializationTest, polymorphism_base_to_base)
{
    std::shared_ptr<Derived> d(std::make_shared<Derived>("foo"));

    typedef ba::text_iarchive IArchive;
    typedef ba::text_oarchive OArchive;

    std::stringstream ss;

    {
        auto b = static_cast<Base*>(d.get());
        OArchive oa(ss);
        oa & b;
    }

    LOG_INFO(ss.str());

    std::shared_ptr<Derived> d2;

    {
        Base* b = nullptr;
        IArchive ia(ss);
        ia & b;

        d2 = std::shared_ptr<Derived>(dynamic_cast<Derived*>(b));
    }

    ASSERT_TRUE(d2 != nullptr);

    EXPECT_EQ(d->bname(), d2->bname());
    EXPECT_EQ(d->dname(), d2->dname());
}

TEST_F(SerializationTest, polymorphism_derived_to_derived)
{
    std::shared_ptr<Derived> d(std::make_shared<Derived>("foo"));

    typedef ba::text_iarchive IArchive;
    typedef ba::text_oarchive OArchive;

    std::stringstream ss;

    {
        OArchive oa(ss);
        Derived* p = d.get();
        oa & p;
    }

    LOG_INFO(ss.str());

    std::shared_ptr<Derived> d2;

    {
        Derived* p = nullptr;
        IArchive ia(ss);
        ia & p;

        ASSERT_TRUE(p != nullptr);
        d2 = std::shared_ptr<Derived>(p);
    }

    EXPECT_EQ(d->bname(), d2->bname());
    EXPECT_EQ(d->dname(), d2->dname());
}

// this one fails to deserialize the Derived part i.e. the dynamic cast fails
TEST_F(SerializationTest, DISABLED_polymorphism_derived_to_base)
{
    std::shared_ptr<Derived> d(std::make_shared<Derived>("foo"));

    typedef ba::text_iarchive IArchive;
    typedef ba::text_oarchive OArchive;

    std::stringstream ss;

    {
        OArchive oa(ss);
        Derived* p = d.get();
        oa & p;
    }

    LOG_INFO(ss.str());

    std::shared_ptr<Derived> d2;
    {
        Base* b = nullptr;
        IArchive ia(ss);
        ia & b;

        ASSERT_TRUE(b != nullptr);

        d2 = std::shared_ptr<Derived>(dynamic_cast<Derived*>(b));
        ASSERT_TRUE(d2 != nullptr);
    }

    EXPECT_EQ(d->bname(), d2->bname());
    EXPECT_EQ(d->dname(), d2->dname());
}

// this segfaults / craps out in a debug build trying to reference an
// inexistent array index
TEST_F(SerializationTest, DISABLED_polymorphism_base_to_derived)
{
    std::shared_ptr<Derived> d(std::make_shared<Derived>("foo"));

    typedef ba::text_iarchive IArchive;
    typedef ba::text_oarchive OArchive;

    std::stringstream ss;

    {
        Base* b = static_cast<Base*>(d.get());
        OArchive oa(ss);
        oa & b;
    }

    LOG_INFO(ss.str());

    std::shared_ptr<Derived> d2;

    {
        Derived* p = nullptr;
        IArchive ia(ss);
        ia & p;
        d2 = std::shared_ptr<Derived>(p);
    }

    ASSERT_TRUE(d2 != nullptr);

    EXPECT_EQ(d->bname(), d2->bname());
    EXPECT_EQ(d->dname(), d2->dname());
}

TEST_F(SerializationTest, two_archives_in_a_row)
{
    typedef ba::text_iarchive IArchive;
    typedef ba::text_oarchive OArchive;

    std::stringstream ss;

    const Base b("foo");
    const Base c("bar");

    {
        OArchive oa(ss);
        oa << b;
    }

    {
        OArchive oa(ss);
        oa << c;
    }

    {
        IArchive ia(ss);
        Base b2;
        ia >> b2;

        EXPECT_EQ(b.bname(), b2.bname());
    }

    {
        IArchive ia(ss);
        Base c2;
        ia >> c2;

        EXPECT_EQ(c.bname(), c2.bname());
    }
}

TEST_F(SerializationTest, two_in_a_row_copied)
{
    typedef ba::text_iarchive IArchive;
    typedef ba::text_oarchive OArchive;

    std::stringstream ss;

    const Base b("foo");
    const Base c("bar");

    {
        OArchive oa(ss);
        oa << b;
    }

    {
        OArchive oa(ss);
        oa << c;
    }

    {
        IArchive ia(ss);
        Base b2;
        ia >> b2;

        EXPECT_EQ(b.bname(), b2.bname());
    }

    std::stringstream ss2;
    ss2 << ss.rdbuf();

    {
        IArchive ia(ss2);
        Base c2;
        ia >> c2;

        EXPECT_EQ(c.bname(), c2.bname());
    }
}

TEST_F(SerializationTest, unique_ptr)
{
    using IArchive = ba::text_iarchive;
    using OArchive = ba::text_oarchive;

    const std::string name("meh");
    std::unique_ptr<Base> base_in(new Derived(name));

    std::stringstream ss;

    {
        OArchive oa(ss);
        oa << base_in;
    }

    std::unique_ptr<Base> base_out;

    {
        IArchive ia(ss);
        ia >> base_out;
    }

    EXPECT_EQ(base_in->bname(),
              base_out->bname());

    EXPECT_EQ(dynamic_cast<const Derived*>(base_in.get())->dname(),
              dynamic_cast<const Derived*>(base_out.get())->dname());
}

}
