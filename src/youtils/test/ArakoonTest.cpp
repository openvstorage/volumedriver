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

#include <boost/asio/ip/tcp.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/regex.hpp>
#include "../ArakoonInterface.h"
#include "../ArakoonTestSetup.h"
#include "../System.h"
#include "../TestBase.h"
#include "../UUID.h"
#include "../wall_timer.h"
#include "../Weed.h"


namespace arakoontest
{
using namespace arakoon;

namespace ara = arakoon;
namespace ba = boost::asio;
namespace yt = youtils;

namespace
{
const std::string arakoon_version(yt::System::get_env_with_default("ARAKOON_VERSION",
                                                                   std::string(".*\\..*\\..*")));
const boost::regex arakoon_hello_string_regex(std::string("Arakoon " + arakoon_version));

}

class ArakoonTest
    : public youtilstest::TestBase
{
public:
    ArakoonTest()
        : test_setup(getTempPath("ArakoonTest") / "arakoon", 3)
    {}

    void
    SetUp()
    {
        test_setup.setUpArakoon();
        logger::setLogging();
    }

    void
    TearDown()
    {
        test_setup.tearDownArakoon();
    }

    void
    test_hello(arakoon::Cluster& cluster)
    {

        EXPECT_TRUE(boost::regex_match(cluster.hello("hullo?"),
                                              arakoon_hello_string_regex));
    }

protected:
    DECLARE_LOGGER("ArakoonTest");

    ara::ArakoonTestSetup test_setup;
};

TEST_F(ArakoonTest, hello)
{
    auto cluster(test_setup.make_client());

    test_hello(*cluster);

    EXPECT_EQ(cluster->who_master(),
              test_setup.nodeID(0).str());
}

TEST_F(ArakoonTest, add_nodes_multiple_times)
{
    auto configs(test_setup.node_configs());
    configs.splice(configs.begin(),
                   test_setup.node_configs());

    ara::Cluster cluster(test_setup.clusterID(),
                         configs);

    test_hello(cluster);
}

TEST_F(ArakoonTest, add_nodes_only_one_node)
{
    auto configs(test_setup.node_configs());
    configs.pop_front();

    EXPECT_THROW(ara::Cluster(test_setup.clusterID(),
                              configs),
                 error_client_unknown_node);

}

TEST_F(ArakoonTest, add_master_only)
{
    // Adding the master node alone works
    ASSERT_LT(0,
              test_setup.num_nodes());

    const std::vector<ara::ArakoonNodeConfig> configs{ test_setup.node_config(0) };
    ara::Cluster cluster(test_setup.clusterID(),
                         configs);

    test_hello(cluster);

    EXPECT_EQ(configs[0].node_id_.str(),
              cluster.who_master());
}

TEST_F(ArakoonTest, add_slave_only)
{
    // Adding the master node alone works
    ASSERT_LT(1,
              test_setup.num_nodes());

    // Adding the master node alone works
    const std::vector<ara::ArakoonNodeConfig> configs{ test_setup.node_config(1) };
    EXPECT_THROW(ara::Cluster(test_setup.clusterID(),
                              configs),
                 ara::error);
}

TEST_F(ArakoonTest, test_connect_wrong_node_id)
{
    std::vector<ara::ArakoonNodeConfig> configs;
    configs.reserve(test_setup.num_nodes());

    for (uint16_t i = 0; i < test_setup.num_nodes(); ++i)
    {
        configs.emplace_back(ara::ArakoonNodeConfig(NodeID(test_setup.nodeID(i) + "*"),
                                                    test_setup.host_name(),
                                                    test_setup.client_port(i)));
    }

    EXPECT_THROW(ara::Cluster(test_setup.clusterID(),
                              configs),
                 error_client_unknown_node);

}

TEST_F(ArakoonTest, test_connect_wrong_node_but_correct_master)
{
    std::vector<ara::ArakoonNodeConfig> configs;
    configs.reserve(test_setup.num_nodes());

    for(uint16_t i = 0; i < test_setup.num_nodes(); ++i)
    {
        configs.emplace_back(ara::ArakoonNodeConfig(NodeID(test_setup.nodeID(i) + (i ? "*" : "")),
                                                    test_setup.host_name(),
                                                    test_setup.client_port(i)));
    }

    ara::Cluster cluster(test_setup.clusterID(),
                         configs);

    test_hello(cluster);

    EXPECT_EQ(cluster.who_master(),
              test_setup.nodeID(0).str());

}

TEST_F(ArakoonTest, test_options)
{
    auto cluster(test_setup.make_client());

    ara::Cluster::MaybeMilliSeconds mms(cluster->timeout());
    ASSERT_EQ(boost::none, mms);

    mms = ara::Cluster::MaybeMilliSeconds(2000);
    cluster->timeout(mms);

    const auto t(cluster->timeout());
    ASSERT_EQ(mms, t);

    // only initialized to make the clang analyzer shut up
    const bool dirty = cluster->allow_dirty();
    cluster->allow_dirty(not dirty);
    EXPECT_NE(dirty,
              cluster->allow_dirty());
}

TEST_F(ArakoonTest, test_exists)
{
    auto cluster(test_setup.make_client());
    const char* key = "this is a key";

    std::string string_key(key);
    EXPECT_FALSE(cluster->exists(string_key));


    void * buf = malloc(string_key.size());
    EXPECT_TRUE(buf);

    memcpy(buf, string_key.data(), string_key.size());

    const buffer buffer_key(buf, string_key.size());

    EXPECT_FALSE(cluster->exists(buffer_key));

    std::vector<uint8_t> vector_key;
    for(unsigned i = 0; i < string_key.length(); ++i)
    {
        vector_key.push_back(string_key[i]);
    }

    EXPECT_FALSE(cluster->exists(vector_key));

    const char* value = "this is a value";

    std::string string_value(value);

    void * buf2 = malloc(string_value.size());
    EXPECT_TRUE(buf2);
    memcpy(buf2, string_value.data(), string_value.size());
    const buffer buffer_value(buf2, string_value.size());

    std::vector<uint8_t> vector_value;
    for(unsigned i = 0; i < string_value.length(); ++i)
    {
        vector_value.push_back(string_value[i]);
    }

    EXPECT_NO_THROW(cluster->set(string_key,
                                string_value));

    EXPECT_TRUE(cluster->exists(string_key));
    EXPECT_TRUE(cluster->exists(buffer_key));
    EXPECT_TRUE(cluster->exists(vector_key));

    EXPECT_NO_THROW(cluster->remove(string_key));

    EXPECT_FALSE(cluster->exists(string_key));
    EXPECT_FALSE(cluster->exists(buffer_key));
    EXPECT_FALSE(cluster->exists(vector_key));

    EXPECT_NO_THROW(cluster->set(vector_key,
                                buffer_value));

    EXPECT_TRUE(cluster->exists(string_key));
    EXPECT_TRUE(cluster->exists(buffer_key));
    EXPECT_TRUE(cluster->exists(vector_key));
}

TEST_F(ArakoonTest, test_value_list)
{
    value_list v_list;
    ASSERT_TRUE(v_list.get());
    ASSERT_EQ(0, v_list.size());
    const void* v_p = v_list.get();


    std::string string_value("value1");

    v_list.add(string_value);

    ASSERT_EQ(1,v_list.size());

    value_list v_list2(std::move(v_list));

    ASSERT_FALSE(v_list.get());
    ASSERT_TRUE(v_list2.get());
    ASSERT_EQ(1,v_list2.size());
    ASSERT_EQ(v_p, v_list2.get());

    std::vector<uint8_t> vector_value(20, 'a');

    v_list2.add(vector_value);
    ASSERT_EQ(2,v_list2.size());

    value_list::iterator it(v_list2.begin());
    arakoon_buffer b;
    for(int i = 0; i < 2; ++i)
    {
        ASSERT_TRUE(it.next(b));
        std::string str((const char*)b.second, b.first);
        switch(i)
        {
        case 0:
            ASSERT_EQ(str,string_value);
            break;
        case 1:
            ASSERT_EQ(str, std::string(20,'a'));
        }
    }

    for(uint64_t i = 0; i < 10; ++i)
    {
        ASSERT_FALSE(it.next(b));
    }
}

// Change to work with random strings
TEST_F(ArakoonTest, gets_and_sets)
{
    auto cluster(test_setup.make_client());

    std::map<std::string, std::string> keyvals;
    const uint64_t test_size = 1024*16;

    for(unsigned i = 0; i < test_size; ++i)
    {
        youtils::UUID key;
        youtils::UUID value;
        cluster->set(key.str(),
                         value.str());
        keyvals[key.str()] = value.str();
        //        std::cout << key.str() << ":" << value.str() << std::endl;
    }
    ASSERT_EQ(test_size,keyvals.size());

    for (const auto& val : keyvals)
    {
        ASSERT_TRUE(cluster->exists(val.first));
        buffer p(cluster->get(val.first));
        ASSERT_EQ(val.second.size(), p.size());
        std::string str((const char*)p.data(), p.size());
        ASSERT_EQ(val.second, str);
    }
}

TEST_F(ArakoonTest, removal_of_nonexistant_entry)
{
    auto cluster(test_setup.make_client());
    std::string some_key("some key");
    ASSERT_FALSE(cluster->exists(some_key));
    EXPECT_THROW(cluster->remove(some_key),
                 error_not_found);
    buffer b;
    EXPECT_NO_THROW(b = cluster->test_and_set(some_key,
                                                  None(),
                                                  None()));
    //    std::cout << std::string((const char*)b.data(), b.size()) << std::endl;
    ASSERT_FALSE(cluster->exists(some_key));


    EXPECT_NO_THROW(b = cluster->test_and_set(some_key,
                                                  None(),
                                                  some_key));
    //    std::cout << std::string((const char*)b.data(), b.size()) << std::endl;

    ASSERT_TRUE(cluster->exists(some_key));

    EXPECT_NO_THROW(b = cluster->test_and_set(some_key,
                                                  None(),
                                                  None()));
    //    std::cout << std::string((const char*)b.data(), b.size()) << std::endl;
    ASSERT_TRUE(cluster->exists(some_key));
    EXPECT_NO_THROW(b = cluster->test_and_set(some_key,
                                                  some_key,
                                                  None()));
    //    std::cout << std::string((const char*)b.data(), b.size()) << std::endl;
    ASSERT_FALSE(cluster->exists(some_key));

    sequence s;
    s.add_delete(some_key);
    ASSERT_THROW(cluster->sequence(s),
                 error_not_found);

}

TEST_F(ArakoonTest, assertions_instead_of_test_and_set)
{
    auto cluster(test_setup.make_client());
    std::string some_key("some key");

    //adding a key, assert it doesnt exists
    //first time => success expected
    {
        sequence s;
        //this is in fact a assert_not_exist
        s.add_assert(some_key, None());
        s.add_set(some_key, std::string("A"));
        ASSERT_NO_THROW(cluster->sequence(s));
    }

    //adding a key, assert if exists
    //second time => assertion expected
    {
        sequence s;
        s.add_assert(some_key, None());
        s.add_set(some_key, std::string("this should fail"));
        ASSERT_THROW(cluster->sequence(s), error_assertion_failed);
    }

    //assert-flavor of test_and_set
    //success expected
    {
        sequence s;
        s.add_assert(some_key, std::string("A"));
        s.add_set(some_key, std::string("B"));
        ASSERT_NO_THROW(cluster->sequence(s));
    }

    //assert-flavor of test_and_set
    //failure expected
    {
        sequence s;
        s.add_assert(some_key, std::string("A"));
        s.add_set(some_key, std::string("C"));
        ASSERT_THROW(cluster->sequence(s), error_assertion_failed);
    }

    //assert-flavor of test_and_set on non-existing key
    //failure expected with error_assertion_failed
    //not with error_not_found
    {
        std::string some_other_key("some other key");
        sequence s;
        s.add_assert(some_other_key, std::string("I'm not here"));
        s.add_set(some_other_key, std::string("this should fail"));
        ASSERT_THROW(cluster->sequence(s), error_assertion_failed);
    }

}

TEST_F(ArakoonTest, DISABLED_test_shootdown_masters)
{
#if 0
    auto cluster(test_setup.make_client());
    const boost::optional<std::string> maybe_master(cluster->who_master());

    ASSERT_TRUE(static_cast<bool>(maybe_master));

    youtils::UUID key;
    youtils::UUID value;
    ASSERT_NO_THROW(cluster->set(key.str(),
                                   value.str()));
    ASSERT_TRUE(cluster->exists(key.str()));

    test_setup.shootDownNode(*maybe_master,
                             SIGHUP);
    ASSERT_THROW(cluster->exists(key.str()),
                 error_client_network_error);
    ASSERT_NO_THROW(cluster->connect_master(100));

    const boost::optional<std::string> maybe_master1(cluster->who_master());
    ASSERT_TRUE(static_cast<bool>(maybe_master1));

    ASSERT_NE(maybe_master, maybe_master1);
    ASSERT_TRUE(cluster->exists(key.str()));
    test_setup.shootDownNode(*maybe_master1,
                             SIGHUP);
    ASSERT_THROW(cluster->exists(key.str()),
                 error_not_master);
    ASSERT_NO_THROW(cluster->connect_master(100));

    const boost::optional<std::string> maybe_master2(cluster->who_master());
    ASSERT_TRUE(static_cast<bool>(maybe_master2));

    ASSERT_NE(maybe_master2, maybe_master1);
    ASSERT_NE(maybe_master2, maybe_master);

    ASSERT_TRUE(cluster->exists(key.str()));

    test_setup.shootDownNode(*maybe_master2,
                             SIGHUP);

    ASSERT_THROW(cluster->connect_master(),
                    error_not_master);
#endif
}

TEST_F(ArakoonTest, prefix)
{
    auto cluster(test_setup.make_client());

    const std::string prefix1("blah/");
    ASSERT_NO_THROW(cluster->delete_prefix("prefix/"));

    std::string hi("ping?");

    ASSERT_NO_THROW(cluster->hello(hi));

    ASSERT_NO_THROW(cluster->prefix(prefix1,
                                    15000));
}

TEST_F(ArakoonTest, delete_prefix)
{
    auto cluster(test_setup.make_client());
    ASSERT_NO_THROW(cluster->delete_prefix("prefix/",
                                           true));

}

TEST_F(ArakoonTest, sequence_api)
{
    auto cluster(test_setup.make_client());

    sequence s1;
    std::string key_prefix("this is key ");
    std::string value_prefix("this is val ");

    for(int i = 0; i < 20; ++i)
    {
        std::stringstream ss_key;
        ss_key << key_prefix << i;
        std::stringstream ss_val;
        ss_val << value_prefix << i;
        s1.add_set(ss_key.str(),
                   ss_val.str());
    }

    cluster->sequence(s1);

    {
        key_value_list kvlist_1 (cluster->range_entries(None(),
                                                        true,
                                                        None(),
                                                        false,
                                                        40));
        ASSERT_EQ(20, kvlist_1.size());
        arakoon_buffer key;
        arakoon_buffer value;
        key_value_list::iterator it(kvlist_1.begin());

        while(it.next(key,
                      value))
        {
            const std::string key_string((const char*)key.second,
                                         key.first);
            const std::string value_string((const char*)value.second,
                                           value.first);
            ASSERT_EQ(0, key_string.find(key_prefix));
            ASSERT_EQ(0, value_string.find(value_prefix));
        }
        for(int i = 0; i < 10; ++i)
        {
            ASSERT_FALSE(it.next(key,
                                 value));
        }
    }

    sequence s2;

    for(int i = 0; i < 20; ++i)
    {
        std::stringstream ss_key;
        ss_key << "this is key " << i;
        s2.add_delete(ss_key.str());
    }

    cluster->sequence(s2);

    {
        key_value_list kvlist_1(cluster->range_entries(None(),
                                                 true,
                                                 None(),
                                                 false,
                                                 40));
        ASSERT_EQ(0, kvlist_1.size());
    }

    cluster->sequence(s1);

    {
       value_list vlist_1 (cluster->prefix(key_prefix,
                                          40));
        ASSERT_EQ(20, vlist_1.size());
        arakoon_buffer key;
        value_list::iterator it(vlist_1.begin());

        while(it.next(key))
        {
            const std::string key_string((const char*)key.second,
                                           key.first);
            ASSERT_EQ(0, key_string.find(key_prefix));
        }
        for(int i = 0; i < 10; ++i)
        {
            ASSERT_FALSE(it.next(key));
        }

        value_list vlist_2(cluster->multi_get(vlist_1));
        ASSERT_EQ(20, vlist_2.size());
        arakoon_buffer val;
        value_list::iterator it2(vlist_2.begin());

        while(it2.next(val))
        {
            const std::string val_string((const char*)val.second,
                                           val.first);
            ASSERT_EQ(0, val_string.find(value_prefix));
        }
        for(int i = 0; i < 10; ++i)
        {
            ASSERT_FALSE(it.next(val));
        }
    }
}

TEST_F(ArakoonTest, sequence_order)
{
    auto cluster(test_setup.make_client());

    arakoon::sequence seq;
    const std::string key = "the meaning of life";
    const std::string val1 = "41";
    const std::string val2 = "42";

    seq.add_set(key, val1);
    seq.add_set(key, val2);

    cluster->sequence(seq);

    auto buf(cluster->get(key));
    EXPECT_EQ(val2.size(), buf.size());

    const std::string res(static_cast<const char*>(buf.data()), buf.size());
    EXPECT_EQ(val2, res);
}

// Disabled for now - it does (did?) show an issue with the C client, but we're
// not using multiget (yet).
TEST_F(ArakoonTest, DISABLED_multi_get_order)
{
    auto cluster(test_setup.make_client());

    unsigned num_keys = 13;
    arakoon::sequence seq;

    std::list<std::string> keys;

    for (unsigned i = 0; i < num_keys; ++i)
    {
        const std::string k(boost::lexical_cast<std::string>(i));
        keys.push_front(k);
        seq.add_set(k, k);
    }

    cluster->sequence(seq);

    arakoon::value_list ara_keys;
    for (const auto& k : keys)
    {
        ara_keys.add(k);
    }

    EXPECT_EQ(num_keys, ara_keys.size());

    auto ara_vals = cluster->multi_get(ara_keys);

    EXPECT_EQ(num_keys, ara_vals.size());

    auto vit = ara_vals.begin();

    for (const auto& k : keys)
    {
        arakoon::arakoon_buffer b;
        vit.next(b);
        EXPECT_EQ(k.size(), b.first);
        const std::string s(static_cast<const char*>(b.second), b.first);
        EXPECT_EQ(k, s);
    }

    {
        arakoon::arakoon_buffer b;
        EXPECT_FALSE(vit.next(b));
        EXPECT_EQ(0, b.first);
        EXPECT_EQ(nullptr, b.second);
    }
}

TEST_F(ArakoonTest, startemup)
{
    if(getenv("RUN_RABBIT_RUN"))
    {
        auto cluster(test_setup.make_client());
        sleep(10000);
    }
}

}

namespace
{
struct Key
{
    Key(const std::string& prefix,
        const uint64_t& addr)
        : size(prefix.size() + sizeof(addr))
        , data((char*)malloc(size))
    {
        ASSERT(not prefix.empty());
        VERIFY(data);
        memcpy(data,
               prefix.c_str(),
               prefix.size());
        *reinterpret_cast<uint64_t*>(data + (size - sizeof(addr))) = addr;
    }

    ~Key()
    {
        free(data);
    }

    Key(const Key&) = delete;
    Key& operator=(const Key*) = delete;

    const size_t size;
    char* data;

    DECLARE_LOGGER("ArakoonTestKey")
};

struct Value
{
    Value(const uint8_t* d,
          uint32_t s)
        : data(d)
        , size(s)
    {}

    const uint8_t* data;
    const uint32_t size;
};

}

namespace arakoon
{

template<>
struct DataBufferTraits<Key>
{
    static size_t
    size(const Key& key)
    {
        return key.size;
    }

    static const void*
    data(const Key& key)
    {
        return key.data;
    }
};

template<>
struct DataBufferTraits<Value>
{
    static size_t
    size(const Value& val)
    {
        return val.size;
    }

    static const void*
    data(const Value& val)
    {
        return val.data;
    }
};

}

namespace arakoontest
{

TEST_F(ArakoonTest, DISABLED_performance)
{
    auto cluster(test_setup.make_client());

    uint32_t vsize = yt::System::get_env_with_default("ARAKOON_VALUE_SIZE",
                                                      static_cast<uint32_t>(32 * 1024));
    uint32_t keys = yt::System::get_env_with_default("ARAKOON_KEY_COUNT",
                                                     static_cast<uint32_t>(10000));
    uint32_t batch_size = yt::System::get_env_with_default("ARAKOON_BATCH_SIZE",
                                                           static_cast<uint32_t>(1));

    assert(batch_size != 0); // Yo, clang analyzer - this one goes out to you.

    ASSERT_TRUE(batch_size != 0) << "arakoon batch size of 0 is not supported";
    ASSERT_EQ(0, keys % batch_size) <<
        "number of keys needs to be a multiple of batch size";

    const std::string pfx("perftest");

    {
        const std::vector<uint8_t> wbuf(vsize);

        yt::wall_timer t;

        std::unique_ptr<ara::sequence> seq;

        for (uint32_t i = 0; i < keys; ++i)
        {
            if (seq == nullptr)
            {
                seq.reset(new ara::sequence());
            }

            Key k(pfx, i);
            Value v(wbuf.data(), wbuf.size());
            seq->add_set(k, v);

            if ((i + 1) % batch_size == 0)
            {
                cluster->sequence(*seq);
                seq.reset();
            }
        }

        LOG_INFO(keys << " sets of " << vsize << " bytes with a batch size of " <<
                 batch_size << " took " << t.elapsed() <<
                 " seconds: " << ((vsize * keys) / (t.elapsed() * 1024 * 1024)) <<
                 " MiB/s / " << keys / t.elapsed() << " IOPS");
    }

    {
        yt::wall_timer t;

        for (uint32_t i = 0; i < keys; ++i)
        {
            Key k(pfx, i);
            ara::buffer b = cluster->get(k);
        }

        LOG_INFO(keys << " gets of " << vsize << " bytes took " << t.elapsed() <<
                 " seconds: " << ((vsize * keys) / (t.elapsed() * 1024 * 1024)) <<
                 " MiB/s / " << keys / t.elapsed() << " IOPS");
    }

    {
        double ara_time = 0;
        yt::wall_timer t;

        std::unique_ptr<ara::value_list> key_list;

        for (uint32_t i = 0; i < keys; ++i)
        {
            if (key_list == nullptr)
            {
                key_list.reset(new ara::value_list());
            }

            key_list->add(Key(pfx, i));

            if ((i + 1) % batch_size == 0)
            {
                yt::wall_timer u;
                auto values = cluster->multi_get(*key_list);
                ara_time += u.elapsed();
                key_list.reset();
            }
        }

        LOG_INFO(keys << " gets of " << vsize << " bytes with a multiget batch size of " <<
                 batch_size << " took " << t.elapsed() <<
                 " seconds: " << ((vsize * keys) / (t.elapsed() * 1024 * 1024)) <<
                 " MiB/s / " << keys / t.elapsed() <<
                 " IOPS, time spent arakooneering: " <<
                 ara_time << " seconds");
    }
}

TEST_F(ArakoonTest, recover_from_reset)
{
    auto cluster(test_setup.make_client());

    const std::string key("some key");
    const std::string val("some value");

    cluster->set(key, val);

    test_setup.stop_nodes();
    test_setup.start_nodes();

    const arakoon::buffer buf(cluster->get(key));
    const std::string res(static_cast<const char*>(buf.data()),
                          buf.size());
    EXPECT_EQ(val, res);
}

TEST_F(ArakoonTest, error_notification)
{
    auto cluster(test_setup.make_client());

    const std::string key("some key");
    const std::string val("some value");

    cluster->set(key, val);

    test_setup.stop_nodes();

    EXPECT_THROW(cluster->get(key),
                 arakoon::error_client_network_error);

    test_setup.start_nodes();

    {
        const arakoon::buffer buf(cluster->get(key));
        const std::string res(static_cast<const char*>(buf.data()),
                              buf.size());
        EXPECT_EQ(val, res);
    }
}

TEST_F(ArakoonTest, connect_timeout)
{
    test_setup.stop_nodes();
    yt::wall_timer wt;
    const uint32_t secs = 5;

    EXPECT_THROW(test_setup.make_client(ara::Cluster::MilliSeconds(secs * 1000)),
                 ara::error);

    const double elapsed = wt.elapsed();
    EXPECT_LT(secs, elapsed);
    EXPECT_GT(2 * secs, elapsed);
}

TEST_F(ArakoonTest, connect_tarpit_timeout)
{
    const auto ncfgs(test_setup.node_configs());
    test_setup.stop_nodes();

    ba::io_service io_service;

    using Acceptor = ba::ip::tcp::acceptor;
    using EndPoint = ba::ip::tcp::endpoint;

    std::vector<Acceptor> acceptors;
    acceptors.reserve(ncfgs.size());

    for (const auto& ncfg : ncfgs)
    {
        Acceptor a(io_service);
        const EndPoint e(ba::ip::tcp::v4(),
                         ncfg.port_);

        a.open(e.protocol());
        a.set_option(Acceptor::reuse_address(true));
        a.bind(e);
        a.listen();

        acceptors.emplace_back(std::move(a));
    }

    yt::wall_timer wt;
    const uint32_t secs = 5;

    EXPECT_THROW(test_setup.make_client(ara::Cluster::MilliSeconds(secs * 1000)),
                 ara::error);

    const double elapsed = wt.elapsed();
    EXPECT_LT(secs, elapsed);
    EXPECT_GT(2 * secs, elapsed);
}

}

// Local Variables: **
// mode: c++ **
// End: **
