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

#include "RegistryTestSetup.h"

#include <youtils/InitializedParam.h>
#include <gtest/gtest.h>
#include <youtils/UUID.h>

#include "../FileSystemParameters.h"
#include "../ObjectRegistration.h"
#include "../ObjectRegistry.h"
#include "../FailOverCacheConfigMode.h"

namespace volumedriverfstest
{

using namespace volumedriverfs;

namespace be = backend;
namespace vd = volumedriver;
namespace yt = youtils;

class ObjectRegistryTest
    : public RegistryTestSetup
{
protected:
    ObjectRegistryTest()
        : RegistryTestSetup("ObjectRegistryTest")
        , cluster_id_("cluster")
        , node_id_("node")
    {}

    virtual void
    SetUp() override
    {
        RegistryTestSetup::SetUp();
        object_registry_.reset(new ObjectRegistry(cluster_id_,
                                                  node_id_,
                                                  registry_));
    }

    std::set<ObjectId>
    fill(const size_t count)
    {
        std::set<ObjectId> vols;

        for (unsigned i = 0; i < count; ++i)
        {
            const std::string s(yt::UUID().str());
            const ObjectId id(s);
            object_registry_->register_base_volume(id,
                                                   be::Namespace(s));

            const auto res(vols.insert(id));
            EXPECT_TRUE(res.second);
        }

        return vols;
    }

    const ClusterId cluster_id_;
    const NodeId node_id_;
    std::unique_ptr<ObjectRegistry> object_registry_;
};

TEST_F(ObjectRegistryTest, register_lookup_and_unregister)
{
    const ObjectId vol_id("volume");

    EXPECT_FALSE(object_registry_->find(vol_id));
    EXPECT_THROW(object_registry_->unregister(vol_id),
                 ObjectNotRegisteredException);

    const be::Namespace nspace;

    object_registry_->register_base_volume(vol_id, nspace);

    ObjectRegistrationPtr reg(object_registry_->find(vol_id));
    ASSERT_TRUE(reg != nullptr);

    EXPECT_EQ(vol_id, reg->volume_id);
    EXPECT_EQ(nspace, reg->getNS());
    EXPECT_EQ(node_id_, reg->node_id);

    object_registry_->unregister(vol_id);
    EXPECT_TRUE(object_registry_->find(vol_id) == nullptr);
    EXPECT_THROW(object_registry_->find_throw(vol_id),
                 ObjectNotRegisteredException);
}

TEST_F(ObjectRegistryTest, destruction)
{
    ASSERT_TRUE(object_registry_->list().empty());

    {
        const be::Namespace nspace;
        const ObjectId oid("volume");

        object_registry_->register_base_volume(oid, nspace);
    }

    const ObjectId tmpl("template");

    {
        const be::Namespace nspace;

        object_registry_->register_base_volume(tmpl, nspace);
        object_registry_->set_volume_as_template(tmpl);
    }

    {
        const be::Namespace nspace;
        const ObjectId oid("clone");

        object_registry_->register_clone(oid,
                                         nspace,
                                         tmpl,
                                         boost::none);
    }

    {
        const be::Namespace nspace;
        const ObjectId oid("file");

        object_registry_->register_file(oid);
    }

    ASSERT_EQ(4U, object_registry_->list().size());

    object_registry_->destroy();

    EXPECT_TRUE(object_registry_->list().empty());
}

TEST_F(ObjectRegistryTest, reregister)
{
    const ObjectId vol_id("volume");
    const be::Namespace nspace;

    object_registry_->register_base_volume(vol_id, nspace);
    EXPECT_THROW(object_registry_->register_base_volume(vol_id,
                                                        nspace),
                 ObjectAlreadyRegisteredException);

    const ObjectRegistrationPtr reg(object_registry_->find(vol_id));
    ASSERT_TRUE(reg != nullptr);

    EXPECT_EQ(vol_id, reg->volume_id);
    EXPECT_EQ(nspace, reg->getNS());
    EXPECT_EQ(node_id_, reg->node_id);

    object_registry_->unregister(vol_id);
    EXPECT_FALSE(object_registry_->find(vol_id));
}

TEST_F(ObjectRegistryTest, migrate_inexistent)
{
    const ObjectId vol_id("volume");

    EXPECT_THROW(object_registry_->migrate(vol_id,
                                           NodeId("from"),
                                           NodeId("to")),
                 ObjectNotRegisteredException);
}

TEST_F(ObjectRegistryTest, migrate_from_wrong_node)
{
    const ObjectId vol_id("volume");
    const be::Namespace nspace;

    object_registry_->register_base_volume(vol_id,
                                           nspace);

    const NodeId node_id("from");
    EXPECT_TRUE(node_id != object_registry_->node_id());

    EXPECT_THROW(object_registry_->migrate(vol_id,
                                           node_id,
                                           object_registry_->node_id()),
                 WrongOwnerException);
}

TEST_F(ObjectRegistryTest, migrate)
{
    const NodeId node("othernode");
    ASSERT_TRUE(object_registry_->node_id() != node);

    ObjectRegistry registry(object_registry_->cluster_id(),
                            node,
                            registry_);

    EXPECT_TRUE(registry.node_id() == node);

    const ObjectId vol_id("volume");
    const be::Namespace nspace;

    registry.register_base_volume(vol_id, nspace);

    ObjectRegistrationPtr reg(object_registry_->find(vol_id));
    ASSERT_TRUE(reg != nullptr);

    EXPECT_TRUE(reg->node_id == registry.node_id());
    EXPECT_TRUE(reg->getNS() == nspace);
    EXPECT_TRUE(reg->volume_id == vol_id);

    object_registry_->migrate(vol_id, node, object_registry_->node_id());

    ObjectRegistrationPtr new_reg(registry.find(vol_id));
    ASSERT_TRUE(new_reg != nullptr);

    EXPECT_TRUE(new_reg->node_id == object_registry_->node_id());
    EXPECT_TRUE(new_reg->getNS() == nspace);
    EXPECT_TRUE(new_reg->volume_id == vol_id);
    EXPECT_NE(reg->owner_tag,
              new_reg->owner_tag);
}

TEST_F(ObjectRegistryTest, unregister_from_wrong_node)
{
    const NodeId node("othernode");
    ASSERT_TRUE(object_registry_->node_id() != node);

    ObjectRegistry registry(object_registry_->cluster_id(),
                            node,
                            registry_);

    EXPECT_TRUE(registry.node_id() == node);

    const ObjectId vol_id("volume");
    const be::Namespace nspace;

    registry.register_base_volume(vol_id, nspace);

    EXPECT_TRUE(object_registry_->find(vol_id) != nullptr);
    EXPECT_THROW(object_registry_->unregister(vol_id),
                 WrongOwnerException);

    registry.unregister(vol_id);
}

TEST_F(ObjectRegistryTest, list_registrations)
{
    const unsigned count = 1000;
    std::set<ObjectId> vols(fill(count));

    ASSERT_EQ(count, vols.size());

    const auto voll(object_registry_->list());

    EXPECT_EQ(count, voll.size());

    for (const auto& v : voll)
    {
        EXPECT_EQ(1U, vols.erase(v)) << "volume id: " << v;
    }

    EXPECT_TRUE(vols.empty());
}

TEST_F(ObjectRegistryTest, get_all_registrations)
{
    ASSERT_TRUE(object_registry_->get_all_registrations(1).empty());

    const size_t count = 128;
    ASSERT_LT(0, count);

    std::set<ObjectId> ids(fill(count));
    ASSERT_EQ(count,
              ids.size());

    const size_t batch_size = count - 1;
    std::vector<ObjectRegistrationPtr> regv(object_registry_->get_all_registrations(batch_size));

    ASSERT_EQ(count,
              regv.size());

    for (const auto& reg : regv)
    {
        EXPECT_EQ(1U,
                  ids.erase(reg->volume_id)) << "volume id: " << reg->volume_id;
    }

    EXPECT_TRUE(ids.empty());
}

TEST_F(ObjectRegistryTest, tree_config)
{
    EXPECT_EQ(ObjectType::Volume,
              ObjectTreeConfig::makeBase().object_type);

    EXPECT_EQ(ObjectType::Template,
              ObjectTreeConfig::makeTemplate(boost::none).object_type);

    EXPECT_EQ(ObjectType::Volume,
              ObjectTreeConfig::makeClone(ObjectId("some_id")).object_type);

    EXPECT_EQ(ObjectType::File,
              ObjectTreeConfig::makeFile().object_type);

    // the following two are rather silly
    EXPECT_EQ(ObjectType::Volume,
              ObjectTreeConfig::makeParent(ObjectType::Volume,
                                           ObjectTreeConfig::Descendants(),
                                           boost::none).object_type);
    EXPECT_EQ(ObjectType::Template,
              ObjectTreeConfig::makeParent(ObjectType::Template,
                                           ObjectTreeConfig::Descendants(),
                                           boost::none).object_type);
}

TEST_F(ObjectRegistryTest, unique_prefix)
{
    const ClusterId cluster_id(yt::UUID().str());
    ObjectRegistry oregistry(cluster_id,
                             node_id_,
                             registry_);

    EXPECT_NE(oregistry.prefix(), object_registry_->prefix());
}

TEST_F(ObjectRegistryTest, foc_config_mode)
{
    const ObjectId vol_id("volume");
    const be::Namespace nspace;
    object_registry_->register_base_volume(vol_id, nspace);

    FailOverCacheConfigMode foc_cm = object_registry_->find(vol_id)->foc_config_mode;
    EXPECT_TRUE(FailOverCacheConfigMode::Automatic == foc_cm);

    object_registry_->set_foc_config_mode(vol_id, FailOverCacheConfigMode::Manual);
    foc_cm = object_registry_->find(vol_id)->foc_config_mode;
    EXPECT_TRUE(FailOverCacheConfigMode::Manual == foc_cm);

    object_registry_->set_foc_config_mode(vol_id, FailOverCacheConfigMode::Automatic);
    foc_cm = object_registry_->find(vol_id)->foc_config_mode;
    EXPECT_TRUE(FailOverCacheConfigMode::Automatic == foc_cm);

    // not much else to do at this level

    object_registry_->unregister(vol_id);
}

}
