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

#include "RegistryTestSetup.h"

#include <boost/thread.hpp>
#include <future>
#include <youtils/InitializedParam.h>
#include <youtils/TestBase.h>
#include <youtils/UUID.h>

#include "../FileSystemParameters.h"
#include "../CachedObjectRegistry.h"
#include "../ObjectRegistration.h"

namespace volumedriverfstest
{

namespace be = backend;
namespace vd = volumedriver;
namespace vfs = volumedriverfs;
namespace yt = youtils;

using namespace std::literals::string_literals;

class CachedObjectRegistryTest
    : public RegistryTestSetup
{
protected:
    CachedObjectRegistryTest()
        : RegistryTestSetup("CachedObjectRegistryTest")
        , cluster_id_("cluster")
        , node_id_("node")
    {}

    virtual void
    SetUp() override
    {
        RegistryTestSetup::SetUp();
        VERIFY(registry_ != nullptr);
        object_registry_.reset(new vfs::CachedObjectRegistry(cluster_id_,
                                                             node_id_,
                                                             registry_));
    }

    std::set<vfs::ObjectId>
    fill_registry(const unsigned count)
    {
        std::set<vfs::ObjectId> vols;

        for (unsigned i = 0; i < count; ++i)
        {
            const std::string s(yt::UUID().str());
            const vfs::ObjectId id(s);
            object_registry_->register_base_volume(id,
                                                   be::Namespace(s));

            const auto res(vols.insert(id));
            EXPECT_TRUE(res.second);
        }

        EXPECT_EQ(count, vols.size());
        return vols;
    }

    void
    test_list_volumes(const unsigned count, bool drop_cache)
    {
        std::set<vfs::ObjectId> vols(fill_registry(count));

        if (drop_cache)
        {
            object_registry_->drop_cache();
        }

        const auto voll(object_registry_->list());

        EXPECT_EQ(count, voll.size());

        for (const auto& v : voll)
        {
            EXPECT_EQ(1U, vols.erase(v)) << "volume id: " << v;
        }

        EXPECT_TRUE(vols.empty());
    }

    const vfs::ClusterId cluster_id_;
    const vfs::NodeId node_id_;
    std::unique_ptr<vfs::CachedObjectRegistry> object_registry_;
};

TEST_F(CachedObjectRegistryTest, register_lookup_and_unregister)
{
    const vfs::ObjectId vol_id("volume");

    EXPECT_FALSE(object_registry_->find(vol_id,
                                        vfs::IgnoreCache::F));
    EXPECT_THROW(object_registry_->unregister(vol_id),
                 vfs::ObjectNotRegisteredException);

    const be::Namespace nspace;

    object_registry_->register_base_volume(vol_id, nspace);

    vfs::ObjectRegistrationPtr reg(object_registry_->find(vol_id,
                                                          vfs::IgnoreCache::F));
    ASSERT_TRUE(reg != nullptr);

    EXPECT_EQ(vol_id, reg->volume_id);
    EXPECT_EQ(nspace, reg->getNS());
    EXPECT_EQ(node_id_, reg->node_id);

    object_registry_->unregister(vol_id);
    EXPECT_TRUE(object_registry_->find(vol_id,
                                       vfs::IgnoreCache::F) == nullptr);
    EXPECT_THROW(object_registry_->find_throw(vol_id,
                                              vfs::IgnoreCache::F),
                 vfs::ObjectNotRegisteredException);
}

TEST_F(CachedObjectRegistryTest, reregister)
{
    const vfs::ObjectId vol_id("volume");
    const be::Namespace nspace;

    object_registry_->register_base_volume(vol_id, nspace);
    EXPECT_THROW(object_registry_->register_base_volume(vol_id, nspace),
                 vfs::ObjectAlreadyRegisteredException);

    const vfs::ObjectRegistrationPtr reg(object_registry_->find(vol_id,
                                                                vfs::IgnoreCache::F));
    ASSERT_TRUE(reg != nullptr);

    EXPECT_EQ(vol_id, reg->volume_id);
    EXPECT_EQ(nspace, reg->getNS());
    EXPECT_EQ(node_id_, reg->node_id);

    object_registry_->unregister(vol_id);
    EXPECT_FALSE(object_registry_->find(vol_id,
                                        vfs::IgnoreCache::F));
}

TEST_F(CachedObjectRegistryTest, migrate_inexistent)
{
    const vfs::ObjectId vol_id("volume");
    const vfs::NodeId from_node("from");
    const vfs::NodeId to_node("from");

    EXPECT_THROW(object_registry_->migrate(vol_id, from_node, to_node),
                 vfs::ObjectNotRegisteredException);
}

TEST_F(CachedObjectRegistryTest, migrate_from_wrong_node)
{
    const vfs::ObjectId vol_id("volume");
    const be::Namespace nspace;

    object_registry_->register_base_volume(vol_id, nspace);

    const vfs::NodeId node_id("from");
    EXPECT_TRUE(node_id != object_registry_->node_id());

    EXPECT_THROW(object_registry_->migrate(vol_id, node_id, object_registry_->node_id()),
                 vfs::WrongOwnerException);
}

TEST_F(CachedObjectRegistryTest, migrate)
{
    const vfs::NodeId remote_node("remote_one");
    vfs::CachedObjectRegistry remote_registry(object_registry_->cluster_id(),
                                              remote_node,
                                              registry_);

    const vfs::ObjectId id("object");
    const be::Namespace nspace;

    object_registry_->register_base_volume(id,
                                           nspace);

    auto check_([&](vfs::CachedObjectRegistry& registry,
                    const vfs::NodeId& exp_node,
                    vfs::IgnoreCache ignore_cache,
                    const std::string& desc)
                {
                    const auto reg(registry.find(id,
                                                 ignore_cache));

                    ASSERT_TRUE(reg != nullptr) << desc;
                    EXPECT_EQ(exp_node,
                              reg->node_id) << desc;
                    EXPECT_EQ(nspace,
                              reg->getNS()) << desc;
                    EXPECT_EQ(id,
                              reg->volume_id) << desc;
                });

    auto check([&](const vfs::NodeId& exp_node,
                   vfs::IgnoreCache ignore_cache,
                   const std::string& desc)
               {
                   check_(*object_registry_,
                          exp_node,
                          ignore_cache,
                          "local: "s + desc);

                   check_(remote_registry,
                          exp_node,
                          ignore_cache,
                          "remote: "s + desc);
               });

    // warm the caches
    check(object_registry_->node_id(),
          vfs::IgnoreCache::F,
          "warm cache");

    object_registry_->migrate(id,
                              object_registry_->node_id(),
                              remote_node);

    // the local one is up-to-date, the remote one not:
    check_(*object_registry_,
           remote_node,
           vfs::IgnoreCache::F,
           "local: read from cache after migration");

    check_(remote_registry,
           object_registry_->node_id(),
           vfs::IgnoreCache::F,
           "remote: read from cache after migration");

    // forcibly access the registry, refreshing the caches
    check(remote_node,
          vfs::IgnoreCache::T,
          "read from backend after migration");

    // the caches should be up-to-date now
    check(remote_node,
          vfs::IgnoreCache::F,
          "re-read from cache after migration");
}

TEST_F(CachedObjectRegistryTest, unregister_from_wrong_node)
{
    const vfs::NodeId node("othernode");
    ASSERT_TRUE(object_registry_->node_id() != node);

    vfs::ObjectRegistry registry(object_registry_->cluster_id(),
                                 node,
                                 registry_);

    EXPECT_TRUE(registry.node_id() == node);

    const vfs::ObjectId vol_id("volume");
    const be::Namespace nspace;

    registry.register_base_volume(vol_id, nspace);

    EXPECT_TRUE(object_registry_->find(vol_id,
                                       vfs::IgnoreCache::F) != nullptr);
    EXPECT_THROW(object_registry_->unregister(vol_id),
                 vfs::WrongOwnerException);

    registry.unregister(vol_id);
}

TEST_F(CachedObjectRegistryTest, list_registrations_warm)
{
    test_list_volumes(1000, false);
}

TEST_F(CachedObjectRegistryTest, list_registrations_cold)
{
    test_list_volumes(1000, true);
}

TEST_F(CachedObjectRegistryTest, cache_warming_and_dropping)
{
    const unsigned count = 100;
    std::set<vfs::ObjectId> vols(fill_registry(count));

    const vfs::NodeId node("node2");
    EXPECT_TRUE(node_id_ != node);

    {
        vfs::CachedObjectRegistry registry(cluster_id_,
                                           node,
                                           registry_);

        for (const auto& id : vols)
        {
            registry.migrate(id, node_id_, node);
        }
    }

    for (const auto& id : vols)
    {
        const vfs::ObjectRegistrationPtr
            reg(object_registry_->find(id,
                                       vfs::IgnoreCache::F));
        ASSERT_TRUE(reg != nullptr);
        EXPECT_EQ(node_id_, reg->node_id);
    }

    object_registry_->drop_cache();

    for (const auto& id : vols)
    {
        const vfs::ObjectRegistrationPtr
            reg(object_registry_->find(id,
                                       vfs::IgnoreCache::F));
        ASSERT_TRUE(reg != nullptr);
        EXPECT_EQ(node, reg->node_id);
    }
}

TEST_F(CachedObjectRegistryTest, forced_backend_access)
{
    const vfs::ObjectId id("volume");
    const be::Namespace nspace;

    object_registry_->register_base_volume(id, nspace);

    const vfs::NodeId node("node2");
    EXPECT_TRUE(node_id_ != node);

    {
        vfs::CachedObjectRegistry registry(cluster_id_,
                                           node,
                                           registry_);

        registry.migrate(id, node_id_, node);
    }

    {
        const vfs::ObjectRegistrationPtr
            reg(object_registry_->find(id,
                                       vfs::IgnoreCache::F));
        ASSERT_TRUE(reg != nullptr);
        ASSERT_EQ(node_id_, reg->node_id);
    }

    {
        const vfs::ObjectRegistrationPtr
            reg(object_registry_->find(id,
                                       vfs::IgnoreCache::T));
        ASSERT_TRUE(reg != nullptr);
        ASSERT_EQ(node, reg->node_id);
    }
}

TEST_F(CachedObjectRegistryTest, volume_tree_mgmt)
{
    const vfs::ObjectId template_id("parent");
    const be::Namespace template_nspace;

    object_registry_->register_base_volume(template_id, template_nspace);
    object_registry_->set_volume_as_template(template_id);
    object_registry_->set_volume_as_template(template_id); //idempotency on set_as_template
    object_registry_->unregister(template_id);

    const vfs::ObjectId clone_id1("clone1");
    const vfs::ObjectId clone_id2("clone2");
    const be::Namespace clone_nspace1;
    const be::Namespace clone_nspace2;

    object_registry_->register_base_volume(template_id, template_nspace);
    //parent not set as template yet
    ASSERT_THROW(object_registry_->register_clone(clone_id1,
                                                  clone_nspace1,
                                                  template_id,
                                                  boost::none),
                 vfs::InvalidOperationException);

    object_registry_->set_volume_as_template(template_id);
    object_registry_->register_clone(clone_id1,
                                     clone_nspace1,
                                     template_id,
                                     boost::none);

    //cannot remove template if it has a clone
    ASSERT_THROW(object_registry_->unregister(template_id),
                 vfs::InvalidOperationException);

    //cannot clone from a clone without snapshot
    ASSERT_THROW(object_registry_->register_clone(vfs::ObjectId("xxx"),
                                                  vd::Namespace(),
                                                  clone_id1,
                                                  boost::none),
                 vfs::InvalidOperationException);

    object_registry_->register_clone(clone_id2,
                                     clone_nspace2,
                                     template_id,
                                     boost::none);

    object_registry_->unregister(clone_id1);

    //template still has clone_id2 as descendant
    ASSERT_THROW(object_registry_->unregister(template_id),
                 vfs::InvalidOperationException);

    object_registry_->unregister(clone_id2);
    object_registry_->unregister(template_id);
}

TEST_F(CachedObjectRegistryTest, concurrent_tree_actions)
{
    const vfs::ObjectId template_id("parent");
    const be::Namespace template_nspace;

    object_registry_->register_base_volume(template_id, template_nspace);
    object_registry_->set_volume_as_template(template_id);

    const uint32_t thread_num = 5;
    const uint32_t iterations = 10;

    std::vector<std::future<void>> futures;

    for(unsigned i = 0; i < thread_num; ++i)
    {
        std::stringstream ss;
        ss << "worker" << i;
        std::string id(ss.str());
        futures.emplace_back(std::async(std::launch::async, [&, id]()
                            {
                                for (uint32_t count = 0; count < iterations; count++)
                                {
                                    object_registry_->register_clone(vfs::ObjectId(id),
                                                                     vd::Namespace(id),
                                                                     template_id,
                                                                     boost::none);
                                    object_registry_->unregister(vfs::ObjectId(id));
                                }
                            }));
    }

    for(auto& fut : futures)
    {
        EXPECT_NO_THROW(fut.get()); //get rethrows exceptions
    }
}

TEST_F(CachedObjectRegistryTest, foc_config_mode)
{
    const vfs::ObjectId vol_id("volume");
    const be::Namespace nspace;
    object_registry_->register_base_volume(vol_id, nspace);

    vfs::FailOverCacheConfigMode foc_cm = object_registry_->find(vol_id, vfs::IgnoreCache::T)->foc_config_mode;
    EXPECT_TRUE(vfs::FailOverCacheConfigMode::Automatic == foc_cm);
    foc_cm = object_registry_->find(vol_id, vfs::IgnoreCache::F)->foc_config_mode;
    EXPECT_TRUE(vfs::FailOverCacheConfigMode::Automatic == foc_cm);

    object_registry_->set_foc_config_mode(vol_id, vfs::FailOverCacheConfigMode::Manual);
    foc_cm = object_registry_->find(vol_id, vfs::IgnoreCache::T)->foc_config_mode;
    EXPECT_TRUE(vfs::FailOverCacheConfigMode::Manual == foc_cm);
    foc_cm = object_registry_->find(vol_id, vfs::IgnoreCache::F)->foc_config_mode;
    EXPECT_TRUE(vfs::FailOverCacheConfigMode::Manual == foc_cm);

    object_registry_->set_foc_config_mode(vol_id, vfs::FailOverCacheConfigMode::Automatic);
    foc_cm = object_registry_->find(vol_id, vfs::IgnoreCache::T)->foc_config_mode;
    EXPECT_TRUE(vfs::FailOverCacheConfigMode::Automatic == foc_cm);
    foc_cm = object_registry_->find(vol_id, vfs::IgnoreCache::F)->foc_config_mode;
    EXPECT_TRUE(vfs::FailOverCacheConfigMode::Automatic == foc_cm);

    // not much else to do at this level

    object_registry_->unregister(vol_id);
}

}
