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

#include <youtils/ArakoonTestSetup.h>
#include <youtils/TestBase.h>
#include <youtils/UUID.h>

#include <boost/lexical_cast.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/thread.hpp>

#include <filesystem/HierarchicalArakoon.h>

namespace volumedriverfstest
{

namespace ara = arakoon;
namespace vfs = volumedriverfs;
namespace yt = youtils;
namespace ytt = youtilstest;

namespace
{
    const vfs::ArakoonPath root("/");
}

struct HArakoonTestData
{
    HArakoonTestData(const std::string& s)
        : name(s)
    {}

    template<typename Archive>
    void
    serialize(Archive& ar, unsigned version)
    {
        CHECK_VERSION(version, 1);
        ar & boost::serialization::make_nvp("name",
                                            const_cast<std::string&>(name));
    }

    bool
    operator==(const HArakoonTestData& other) const
    {
        return name == other.name;
    }

    bool
    operator!=(const HArakoonTestData& other) const
    {
        return not operator==(other);
    }

    const std::string name;
};

std::ostream&
operator<<(std::ostream& os, const HArakoonTestData& d)
{
    return os << d.name;
}

struct HArakoonUUIDTestData
{
    HArakoonUUIDTestData(const std::string& s)
        : name(s)
    {}

    template<typename Archive>
    void
    serialize(Archive& ar, unsigned version)
    {
        CHECK_VERSION(version, 1);
        ar & boost::serialization::make_nvp("name",
                                            const_cast<std::string&>(name));
    }

    bool
    operator==(const HArakoonUUIDTestData& other) const
    {
        return name == other.name;
    }

    bool
    operator!=(const HArakoonUUIDTestData& other) const
    {
        return not operator==(other);
    }

    const std::string name;
};

std::ostream&
operator<<(std::ostream& os, const HArakoonUUIDTestData& d)
{
    return os << d.name;
}

}

namespace volumedriverfs
{

template<>
struct
HierarchicalArakoonUUIDAllocator<volumedriverfstest::HArakoonUUIDTestData>
{
    static youtils::UUID alloc_uuid(const volumedriverfstest::HArakoonUUIDTestData& data)
    {
        return youtils::UUID(data.name);
    };
};

}

namespace boost
{

namespace serialization
{

template<typename Archive>
inline void
load_construct_data(Archive&,
                    volumedriverfstest::HArakoonTestData* data,
                    unsigned /* version */)
{
    new (data) volumedriverfstest::HArakoonTestData(volumedriverfs::ArakoonEntryId("uninitialized!"));
}

template<typename Archive>
inline void
load_construct_data(Archive&,
                    volumedriverfstest::HArakoonUUIDTestData* data,
                    unsigned /* version */)
{
    new (data) volumedriverfstest::HArakoonUUIDTestData(volumedriverfs::ArakoonEntryId("uninitialized!"));
}

}

}

BOOST_CLASS_VERSION(volumedriverfstest::HArakoonTestData, 1);
BOOST_CLASS_VERSION(volumedriverfstest::HArakoonUUIDTestData, 1);

namespace volumedriverfstest
{

class HierarchicalArakoonTest
    : public ytt::TestBase
    , public ara::ArakoonTestSetup
{
protected:
    HierarchicalArakoonTest()
        : ytt::TestBase()
        , ara::ArakoonTestSetup(getTempPath("HierarchicalArakoonTest") / "arakoon")
        , prefix_("harakoon_test")
    {}

    void
    SetUp()
    {
        setUpArakoon();

        larakoon_ = std::make_shared<vfs::LockedArakoon>(clusterID(),
                                                         node_configs());
        harakoon_.reset(new vfs::HierarchicalArakoon(larakoon_,
                                                     prefix_));

    }

    void
    TearDown()
    {
        harakoon_.reset();
        larakoon_ = nullptr;

        tearDownArakoon();
    }

    void
    initialize(const std::string& meta = "root metadata")
    {
        harakoon_->initialize(HArakoonTestData(meta));
    }

    void
    initialize(const youtils::UUID& uuid)
    {
        harakoon_->initialize<HArakoonUUIDTestData>(HArakoonUUIDTestData(uuid.str()));
    }

    void
    validate_path(const vfs::ArakoonPath& path)
    {
        vfs::HierarchicalArakoon::validate_path_(path);
    }

    const std::string prefix_;
    std::shared_ptr<vfs::LockedArakoon> larakoon_;
    std::unique_ptr<vfs::HierarchicalArakoon> harakoon_;
};

TEST_F(HierarchicalArakoonTest, construction)
{}

TEST_F(HierarchicalArakoonTest, initialization)
{
    EXPECT_FALSE(harakoon_->initialized());

    const std::string s("some metadata for the root node");
    initialize(s);

    EXPECT_TRUE(harakoon_->initialized());

    auto val(harakoon_->get<HArakoonTestData>(vfs::ArakoonPath("/")));
    EXPECT_EQ(s, val->name);
}

TEST_F(HierarchicalArakoonTest, uuid_initialization)
{
    EXPECT_FALSE(harakoon_->initialized());

    const youtils::UUID init_uuid;
    initialize(init_uuid);

    EXPECT_TRUE(harakoon_->initialized());

    auto val(harakoon_->get<HArakoonTestData>(vfs::ArakoonPath("/")));
    EXPECT_EQ(init_uuid.str(), val->name);
}

TEST_F(HierarchicalArakoonTest, destruction)
{
    {
        const ara::value_list l(larakoon_->prefix(prefix_));
        ASSERT_EQ(0U, l.size());
    }

    const std::string s("some metadata for the root node");
    initialize(s);

    {
        ara::value_list l(larakoon_->prefix(prefix_));
        ASSERT_LT(0U, l.size());
    }

    vfs::HierarchicalArakoon::destroy(larakoon_, prefix_);

    ara::value_list l(larakoon_->prefix(prefix_));
    EXPECT_EQ(0U, l.size());
}

TEST_F(HierarchicalArakoonTest, setting_and_getting)
{
    initialize();

    const vfs::ArakoonPath parent("/foo");
    const vfs::ArakoonPath child(parent / "bar");

    EXPECT_THROW(harakoon_->get<HArakoonTestData>(parent),
                 vfs::HierarchicalArakoon::DoesNotExistException);

     EXPECT_THROW(harakoon_->get<HArakoonTestData>(child),
                  vfs::HierarchicalArakoon::DoesNotExistException);

    const HArakoonTestData pdata("foo");
    harakoon_->set(parent, pdata);

    const HArakoonTestData cdata1("foobar");
    harakoon_->set(child, cdata1);

    {
        auto ptr(harakoon_->get<HArakoonTestData>(child));
        EXPECT_EQ(cdata1.name, ptr->name);
    }

    const HArakoonTestData cdata2("fubar");
    harakoon_->set(child, cdata2);

    {
        auto ptr(harakoon_->get<HArakoonTestData>(child));
        EXPECT_EQ(cdata2.name, ptr->name);
    }

    {
        auto ptr(harakoon_->get<HArakoonTestData>(parent));
        EXPECT_EQ(pdata.name, ptr->name);
    }
}

TEST_F(HierarchicalArakoonTest, uuid_setting_and_getting)
{
    youtils::UUID init_uuid;
    initialize(init_uuid);

    auto root_ptr(harakoon_->get<HArakoonUUIDTestData>(vfs::ArakoonPath("/")));
    youtils::UUID parent_uuid(root_ptr->name);

    youtils::UUID foo_uuid;

    EXPECT_THROW(harakoon_->get<HArakoonUUIDTestData>(foo_uuid),
                 vfs::HierarchicalArakoon::DoesNotExistException);

    const HArakoonUUIDTestData pdata(foo_uuid.str());
    const std::string name("foo");
    harakoon_->set(parent_uuid, name, pdata);

    {
        auto foo_ptr(harakoon_->get<HArakoonUUIDTestData>(vfs::ArakoonPath("/foo")));
        EXPECT_EQ(pdata.name, foo_ptr->name);
    }

    {
        auto foo_uuid_ptr(harakoon_->get<HArakoonUUIDTestData>(foo_uuid));
        EXPECT_EQ(pdata.name, foo_uuid_ptr->name);
    }
}

TEST_F(HierarchicalArakoonTest, path_validation)
{
#define OK(path)                                                        \
    EXPECT_NO_THROW(validate_path(vfs::ArakoonPath(path))) << (path) << \
        " is supposed to be valid"

#define NOK(path)                                                       \
    EXPECT_THROW(validate_path(vfs::ArakoonPath(path)),                 \
                 vfs::HierarchicalArakoon::InvalidPathException) << path << \
        " is not supposed to be valid"

    OK("/");
    OK("/foo");
    OK("/foo/bar");
    OK("/foo/.bar");
    OK("/foo/b.a.r.");
    OK("/foo/bar..");
    OK("/foo/b..ar");
    OK("/foo/b.ar");

    NOK("");
    NOK(".");
    NOK("..");
    NOK("./foo");
    NOK("../foo");
    NOK("/foo/./bar");
    NOK("/foo/../bar");
    NOK("foo");
    NOK("foo/");
    NOK("/foo/");

#undef NOK
#undef OK
}

TEST_F(HierarchicalArakoonTest, listing)
{
    initialize();

    const unsigned count = 101;

    std::set<std::string> set;

    for (unsigned i = 0; i < count; ++i)
    {
        const std::string s(boost::lexical_cast<std::string>(i));
        set.insert(s);
        const vfs::ArakoonPath p(root / s);
        harakoon_->set(p, HArakoonTestData(s));
    }

    ASSERT_EQ(count, set.size());

    const std::list<std::string> l(harakoon_->list(root));
    EXPECT_EQ(count, l.size());

    for (const auto& e : l)
    {
        set.erase(e);
    }

    EXPECT_TRUE(set.empty());
}

TEST_F(HierarchicalArakoonTest, uuid_listing)
{
    youtils::UUID init_uuid;
    initialize(init_uuid);

    auto root_ptr(harakoon_->get<HArakoonUUIDTestData>(vfs::ArakoonPath("/")));
    youtils::UUID parent_uuid(root_ptr->name);

    const unsigned count = 101;

    std::map<std::string, youtils::UUID> map;

    for (unsigned i = 0; i < count; i++)
    {
        const std::string s(boost::lexical_cast<std::string>(i));
        const youtils::UUID p;
        map.insert(std::make_pair(s, p));
        harakoon_->set(parent_uuid, s, HArakoonUUIDTestData(p.str()));
    }

    ASSERT_EQ(count, map.size());

    const std::list<std::string> l(harakoon_->list(parent_uuid));
    EXPECT_EQ(count, l.size());

    for (const auto& e : l)
    {
        map.erase(e);
    }

    EXPECT_TRUE(map.empty());
}

TEST_F(HierarchicalArakoonTest, erasure)
{
    initialize();

    const HArakoonTestData p("parent");
    const vfs::ArakoonPath parent(root / p.name);

    EXPECT_THROW(harakoon_->erase(parent),
                 vfs::HierarchicalArakoon::DoesNotExistException);

    harakoon_->set(parent, p);

    const HArakoonTestData c("child");
    const vfs::ArakoonPath child(parent / c.name);
    harakoon_->set(child, c);

    EXPECT_THROW(harakoon_->erase(parent),
                 vfs::HierarchicalArakoon::NotEmptyException);

    {
        const auto val(harakoon_->get<HArakoonTestData>(child));
        EXPECT_EQ(c, *val);
    }

    {
        const auto val(harakoon_->get<HArakoonTestData>(parent));
        EXPECT_EQ(p, *val);
    }

    harakoon_->erase(child);

    EXPECT_THROW(harakoon_->get<HArakoonTestData>(child),
                 vfs::HierarchicalArakoon::DoesNotExistException);

    harakoon_->erase(parent);
    EXPECT_THROW(harakoon_->get<HArakoonTestData>(parent),
                 vfs::HierarchicalArakoon::DoesNotExistException);
}

TEST_F(HierarchicalArakoonTest, uuid_erasure)
{
    youtils::UUID init_uuid;
    initialize(init_uuid);

    auto root_ptr(harakoon_->get<HArakoonUUIDTestData>(vfs::ArakoonPath("/")));
    youtils::UUID parent_uuid(root_ptr->name);

    const std::string child1("child1");
    const youtils::UUID child1_uuid;

    EXPECT_THROW(harakoon_->erase(parent_uuid, child1),
            vfs::HierarchicalArakoon::DoesNotExistException);

    harakoon_->set(parent_uuid, child1, HArakoonUUIDTestData(child1_uuid.str()));

    const std::string child2("child2");
    const youtils::UUID child2_uuid;

    harakoon_->set(child1_uuid, child2, HArakoonUUIDTestData(child2_uuid.str()));

    EXPECT_THROW(harakoon_->erase(parent_uuid, child1),
            vfs::HierarchicalArakoon::NotEmptyException);

    {
        const auto val(harakoon_->get<HArakoonUUIDTestData>(child1_uuid));
        EXPECT_EQ(child1_uuid.str(), val->name);
    }

    {
        const auto val(harakoon_->get<HArakoonUUIDTestData>(child2_uuid));
        EXPECT_EQ(child2_uuid.str(), val->name);
    }

    harakoon_->erase(child1_uuid, child2);

    EXPECT_THROW(harakoon_->get<HArakoonUUIDTestData>(child2_uuid),
            vfs::HierarchicalArakoon::DoesNotExistException);

    harakoon_->erase(parent_uuid, child1);
    EXPECT_THROW(harakoon_->get<HArakoonUUIDTestData>(child1_uuid),
            vfs::HierarchicalArakoon::DoesNotExistException);
}

TEST_F(HierarchicalArakoonTest, rename)
{
    initialize();

    const HArakoonTestData foo("foo");
    const vfs::ArakoonPath foop(root / foo.name);

    const HArakoonTestData bar("bar");
    const vfs::ArakoonPath barp(root / bar.name);

    // neither src nor dst exist
    EXPECT_THROW(harakoon_->rename<HArakoonTestData>(foop, barp),
                 vfs::HierarchicalArakoon::DoesNotExistException);

    harakoon_->set(foop, foo);

    // src and dst are the same
    EXPECT_NO_THROW(harakoon_->rename<HArakoonTestData>(foop, foop));

    {
        const auto val(harakoon_->get<HArakoonTestData>(foop));
        EXPECT_EQ(foo, *val);
    }

    // src is a prefix of dst
    EXPECT_THROW(harakoon_->rename<HArakoonTestData>(foop, vfs::ArakoonPath(foop / "baz")),
                 vfs::HierarchicalArakoon::InvalidPathException);

    harakoon_->set(barp, bar);

    // dst already exists - and should be overwritten.
    boost::optional<boost::shared_ptr<HArakoonTestData> >
        old(harakoon_->rename<HArakoonTestData>(foop,
                                                barp));

    EXPECT_EQ(**old, bar);

    {
        const auto val(harakoon_->get<HArakoonTestData>(barp));
        EXPECT_EQ(foo, *val);
    }

    EXPECT_THROW(harakoon_->get<HArakoonTestData>(foop),
                 vfs::HierarchicalArakoon::DoesNotExistException);

    const vfs::ArakoonPath bazp(root / "baz");

    EXPECT_EQ(boost::none, harakoon_->rename<HArakoonTestData>(barp, bazp));

    EXPECT_THROW(harakoon_->get<HArakoonTestData>(barp),
                 vfs::HierarchicalArakoon::DoesNotExistException);

    {
        const auto val(harakoon_->get<HArakoonTestData>(bazp));
        EXPECT_EQ(foo, *val);
    }
}

TEST_F(HierarchicalArakoonTest, uuid_rename)
{
    youtils::UUID init_uuid;
    initialize(init_uuid);

    auto root_ptr(harakoon_->get<HArakoonUUIDTestData>(vfs::ArakoonPath("/")));
    youtils::UUID parent_uuid(root_ptr->name);

    const std::string foop("foo");
    const std::string barp("bar");

    // neither src nor dst exist
    EXPECT_THROW(harakoon_->rename<HArakoonUUIDTestData>(parent_uuid,
                                                         foop,
                                                         parent_uuid,
                                                         barp),
            vfs::HierarchicalArakoon::DoesNotExistException);

    const youtils::UUID foop_uuid;
    harakoon_->set(parent_uuid, foop, HArakoonUUIDTestData(foop_uuid.str()));

    // src and dst are the same
    EXPECT_NO_THROW(harakoon_->rename<HArakoonUUIDTestData>(parent_uuid,
                                                            foop,
                                                            parent_uuid,
                                                            foop));

    {
        const auto val(harakoon_->get<HArakoonUUIDTestData>(foop_uuid));
        EXPECT_EQ(foop_uuid.str(), val->name);
    }

    const std::string bazp("baz");
    // src is a prefix of dst
    EXPECT_THROW(harakoon_->rename<HArakoonUUIDTestData>(parent_uuid,
                                                         foop,
                                                         foop_uuid,
                                                         bazp),
                 vfs::HierarchicalArakoon::InvalidPathException);

    const youtils::UUID barp_uuid;
    harakoon_->set(parent_uuid, barp, HArakoonUUIDTestData(barp_uuid.str()));

    //dst already exists - and should be overwritten
    boost::optional<boost::shared_ptr<HArakoonUUIDTestData> > old(
            harakoon_->rename<HArakoonUUIDTestData>(parent_uuid,
                              foop,
                              parent_uuid,
                              barp));
    EXPECT_EQ((*old)->name, barp_uuid.str());

    {
        const auto valu(harakoon_->get<HArakoonUUIDTestData>(foop_uuid));
        EXPECT_EQ(foop_uuid.str(), valu->name);

        const auto valp(harakoon_->get<HArakoonTestData>(vfs::ArakoonPath(root / "bar")));
        EXPECT_EQ(foop_uuid.str(), valp->name);
    }

    {
        EXPECT_THROW(harakoon_->get<HArakoonUUIDTestData>(barp_uuid),
                vfs::HierarchicalArakoon::DoesNotExistException);

        EXPECT_THROW(harakoon_->get<HArakoonTestData>(vfs::ArakoonPath(root / "foo")),
                vfs::HierarchicalArakoon::DoesNotExistException);
    }

    EXPECT_EQ(boost::none, harakoon_->rename<HArakoonUUIDTestData>(parent_uuid,
                                                                   barp,
                                                                   parent_uuid,
                                                                   bazp));

    EXPECT_THROW(harakoon_->get<HArakoonTestData>(vfs::ArakoonPath(root / "bar")),
                vfs::HierarchicalArakoon::DoesNotExistException);

    {
        const auto valu(harakoon_->get<HArakoonUUIDTestData>(foop_uuid));
        EXPECT_EQ(foop_uuid.str(), valu->name);

        const auto valp(harakoon_->get<HArakoonTestData>(vfs::ArakoonPath(root / "baz")));
        EXPECT_EQ(foop_uuid.str(), valp->name);
    }
}

TEST_F(HierarchicalArakoonTest, get_path_by_id)
{
    youtils::UUID init_uuid;
    initialize(init_uuid);

    auto root_ptr(harakoon_->get<HArakoonUUIDTestData>(vfs::ArakoonPath("/")));
    youtils::UUID parent_uuid(root_ptr->name);

    const std::string varp("var");
    const std::string tmpp("tmp");
    const std::string newp("new");
    const std::string oldfilep("oldfile.txt");
    const std::string newfilep("newfile.txt");
    const std::string oldpath("/var/tmp/oldfile.txt");
    const std::string newpath("/var/new/newfile.txt");

    const youtils::UUID varp_id;
    const youtils::UUID tmpp_id;
    const youtils::UUID newp_id;
    const youtils::UUID filep_id;

    harakoon_->set(parent_uuid, varp, HArakoonUUIDTestData(varp_id.str()));

    harakoon_->set(varp_id, tmpp, HArakoonUUIDTestData(tmpp_id.str()));
    harakoon_->set(varp_id, newp, HArakoonUUIDTestData(newp_id.str()));

    harakoon_->set(tmpp_id, oldfilep, HArakoonUUIDTestData(filep_id.str()));

    EXPECT_EQ(oldpath, harakoon_->get_path_by_id(filep_id));

    harakoon_->rename<HArakoonUUIDTestData>(tmpp_id,
                                            oldfilep,
                                            newp_id,
                                            newfilep);

    EXPECT_EQ(newpath, harakoon_->get_path_by_id(filep_id));

    harakoon_->rename<HArakoonUUIDTestData>(newp_id,
                                            newfilep,
                                            newp_id,
                                            oldfilep);

    const std::string npath("/var/new/oldfile.txt");
    EXPECT_EQ(npath, harakoon_->get_path_by_id(filep_id));

    EXPECT_NO_THROW(harakoon_->rename<HArakoonTestData>(vfs::ArakoonPath(npath),
                                                        vfs::ArakoonPath(oldpath)));
    EXPECT_EQ(oldpath, harakoon_->get_path_by_id(filep_id));

    const youtils::UUID oldpath_id(harakoon_->get_id_by_path(vfs::ArakoonPath(oldpath)));
    EXPECT_EQ(filep_id, oldpath_id);

    harakoon_->erase(tmpp_id, oldfilep);
    EXPECT_THROW(harakoon_->get_path_by_id(filep_id),
                 vfs::HierarchicalArakoon::DoesNotExistException);

    EXPECT_EQ("/", harakoon_->get_path_by_id(parent_uuid));
}

TEST_F(HierarchicalArakoonTest, stress)
{
    initialize();

    const unsigned ops_per_worker = 128;

    auto worker([&]
                {
                    for (unsigned i = 0; i < ops_per_worker; ++i)
                    {
                        const std::string f(yt::UUID().str());
                        const vfs::ArakoonPath from(root / f);
                        const HArakoonTestData data(f);

                        harakoon_->set(from, data);

                        const std::string t(yt::UUID().str());
                        const vfs::ArakoonPath to(root / t);

                        harakoon_->rename<HArakoonTestData>(from, to);

                        const auto val(harakoon_->get<HArakoonTestData>(to));
                        EXPECT_EQ(data, *val);

                        harakoon_->erase(to);
                    }
                });

    const unsigned num_workers =
        std::max(2L, std::min(::sysconf(_SC_NPROCESSORS_ONLN), 8L));

    std::vector<boost::thread> workers(num_workers);

    for (unsigned i = 0; i < num_workers; ++i)
    {
        workers.push_back(boost::thread(worker));
    }

    for (auto& w : workers)
    {
        w.join();
    }

    EXPECT_TRUE(harakoon_->list(root).empty());
}

TEST_F(HierarchicalArakoonTest, uuid_stress)
{
    youtils::UUID init_uuid;
    initialize(init_uuid);

    auto root_ptr(harakoon_->get<HArakoonUUIDTestData>(vfs::ArakoonPath("/")));
    youtils::UUID parent_uuid(root_ptr->name);

    const unsigned ops_per_worker = 128;

    auto worker([&]
            {
                for (unsigned i = 0; i < ops_per_worker; i++)
                {
                    const youtils::UUID f_uuid;
                    const std::string f(f_uuid.str());
                    const HArakoonUUIDTestData data(f);

                    harakoon_->set<HArakoonUUIDTestData>(parent_uuid,
                                                         f,
                                                         data);

                    const youtils::UUID t_uuid;
                    const std::string t(t_uuid.str());

                    harakoon_->rename<HArakoonUUIDTestData>(parent_uuid,
                                                         f,
                                                         parent_uuid,
                                                         t);

                    const auto val(harakoon_->get<HArakoonUUIDTestData>(f_uuid));
                    EXPECT_EQ(data, *val);

                    harakoon_->erase(parent_uuid, t);
                }
            });

    const unsigned num_workers =
        std::max(2L, std::min(::sysconf(_SC_NPROCESSORS_ONLN), 8L));

    std::vector<boost::thread> workers(num_workers);

    for (unsigned i = 0; i < num_workers; ++i)
    {
        workers.push_back(boost::thread(worker));
    }

    for (auto& w : workers)
    {
        w.join();
    }

    EXPECT_TRUE(harakoon_->list(parent_uuid).empty());
}

namespace
{

struct BogusHArakoonTestData
{
    BogusHArakoonTestData()
        : num(12345678ULL)
        , str("uninitialized")
    {}

    template<typename Archive>
    void
    serialize(Archive& ar, unsigned version)
    {
        CHECK_VERSION(version, 0);

        ar & boost::serialization::make_nvp("num",
                                            const_cast<uint64_t&>(num));
        ar & boost::serialization::make_nvp("str",
                                            const_cast<std::string&>(str));
    }

    const uint64_t num;
    const std::string str;
};

}

}

BOOST_CLASS_VERSION(volumedriverfstest::BogusHArakoonTestData, 0);

namespace volumedriverfstest
{

TEST_F(HierarchicalArakoonTest, get_wrong_type)
{
    initialize();

    const vfs::ArakoonPath p(root / "foo");

    harakoon_->set(p, HArakoonTestData("foo"));

    EXPECT_THROW(harakoon_->get<BogusHArakoonTestData>(p),
                 std::exception);
}

TEST_F(HierarchicalArakoonTest, uuid_get_wrong_type)
{
    youtils::UUID init_uuid;
    initialize(init_uuid);

    auto root_ptr(harakoon_->get<HArakoonUUIDTestData>(vfs::ArakoonPath("/")));
    youtils::UUID parent_uuid(root_ptr->name);

    const youtils::UUID foo_uuid;
    const std::string foop("foo");

    harakoon_->set<HArakoonUUIDTestData>(parent_uuid, foop,
                                         HArakoonUUIDTestData(foo_uuid.str()));

    EXPECT_THROW(harakoon_->get<BogusHArakoonTestData>(foo_uuid),
                 std::exception);
}

TEST_F(HierarchicalArakoonTest, test_and_set)
{
    initialize();

    const vfs::ArakoonPath p(root / "foo");

    const HArakoonTestData oldval("old");

    EXPECT_THROW(harakoon_->test_and_set(p,
                                         oldval,
                                         boost::optional<HArakoonTestData>(oldval)),
                 vfs::HierarchicalArakoon::PreconditionFailedException);

    harakoon_->test_and_set(p,
                            oldval,
                            boost::optional<HArakoonTestData>());

    EXPECT_EQ(oldval, *harakoon_->get<HArakoonTestData>(p));

    const HArakoonTestData newval("new");

    EXPECT_THROW(harakoon_->test_and_set(p,
                                         newval,
                                         boost::optional<HArakoonTestData>()),
                 vfs::HierarchicalArakoon::PreconditionFailedException);

    EXPECT_THROW(harakoon_->test_and_set(p,
                                         newval,
                                         boost::optional<HArakoonTestData>(newval)),
                 vfs::HierarchicalArakoon::PreconditionFailedException);

    EXPECT_EQ(oldval, *harakoon_->get<HArakoonTestData>(p));

    harakoon_->test_and_set(p, newval, boost::optional<HArakoonTestData>(oldval));

    EXPECT_EQ(newval, *harakoon_->get<HArakoonTestData>(p));
}

TEST_F(HierarchicalArakoonTest, uuid_test_and_set)
{
    youtils::UUID init_uuid;
    initialize(init_uuid);

    auto root_ptr(harakoon_->get<HArakoonUUIDTestData>(vfs::ArakoonPath("/")));
    youtils::UUID parent_uuid(root_ptr->name);

    const youtils::UUID p_uuid;
    const std::string p("foo");

    const HArakoonUUIDTestData oldval(p_uuid.str());

    EXPECT_THROW(harakoon_->test_and_set<HArakoonUUIDTestData>(parent_uuid,
                                         p,
                                         oldval,
                                         boost::optional<HArakoonUUIDTestData>(oldval)),
            vfs::HierarchicalArakoon::PreconditionFailedException);

    harakoon_->test_and_set<HArakoonUUIDTestData>(parent_uuid,
                                                  p,
                                                  oldval,
                                                  boost::optional<HArakoonUUIDTestData>());

    EXPECT_EQ(oldval, *harakoon_->get<HArakoonUUIDTestData>(p_uuid));

    const youtils::UUID n_uuid;
    const HArakoonUUIDTestData newval(n_uuid.str());

    EXPECT_THROW(harakoon_->test_and_set(parent_uuid,
                                         p,
                                         newval,
                                         boost::optional<HArakoonUUIDTestData>()),
            vfs::HierarchicalArakoon::PreconditionFailedException);

    EXPECT_THROW(harakoon_->test_and_set(parent_uuid,
                                         p,
                                         newval,
                                         boost::optional<HArakoonUUIDTestData>(newval)),
            vfs::HierarchicalArakoon::PreconditionFailedException);

    EXPECT_EQ(oldval, *harakoon_->get<HArakoonUUIDTestData>(p_uuid));

    // newval is set but the UUID is kept the same
    harakoon_->test_and_set<HArakoonUUIDTestData>(parent_uuid,
                                                  p,
                                                  newval,
                                                  boost::optional<HArakoonUUIDTestData>(oldval));

    EXPECT_EQ(newval, *harakoon_->get<HArakoonUUIDTestData>(p_uuid));

    const youtils::UUID b_uuid;
    const std::string b("bar");

    const HArakoonUUIDTestData b_oldval(b_uuid.str());
    const HArakoonUUIDTestData b_newval(youtils::UUID().str());

    EXPECT_THROW(harakoon_->test_and_set(b_uuid, b_oldval, b_newval),
            vfs::HierarchicalArakoon::DoesNotExistException);

    harakoon_->test_and_set<HArakoonUUIDTestData>(parent_uuid,
                                                  b,
                                                  b_oldval,
                                                  boost::optional<HArakoonUUIDTestData>());

    EXPECT_EQ(b_oldval, *harakoon_->get<HArakoonUUIDTestData>(b_uuid));

    {
        const auto valp(harakoon_->get<HArakoonTestData>(vfs::ArakoonPath(root / "bar")));
        EXPECT_EQ(b_oldval, valp->name);
    }

    EXPECT_THROW(harakoon_->test_and_set(b_uuid, b_newval, b_newval),
            vfs::HierarchicalArakoon::PreconditionFailedException);

    EXPECT_NO_THROW(harakoon_->test_and_set(b_uuid, b_newval, b_oldval));

    EXPECT_EQ(b_newval, *harakoon_->get<HArakoonUUIDTestData>(b_uuid));

    {
        const auto valp(harakoon_->get<HArakoonTestData>(vfs::ArakoonPath(root / "bar")));
        EXPECT_EQ(b_newval, valp->name);
    }
}

}
