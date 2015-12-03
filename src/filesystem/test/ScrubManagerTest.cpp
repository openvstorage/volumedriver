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

#include "../ClusterId.h"
#include "../NodeId.h"
#include "../ObjectRegistry.h"
#include "../ScrubManager.h"

#include <future>

#include <youtils/SourceOfUncertainty.h>

#include <backend/Garbage.h>

#include <volumedriver/SnapshotName.h>

namespace volumedriverfstest
{

using namespace std::literals::string_literals;
using namespace volumedriverfs;

namespace ara = arakoon;
namespace be = backend;
namespace vd = volumedriver;
namespace yt = youtils;

namespace
{

auto apply_scrub_reply_nop([](const ObjectId&,
                              const scrubbing::ScrubReply&,
                              const vd::ScrubbingCleanup) -> ScrubManager::MaybeGarbage
                           {
                               return boost::none;
                           });

auto collect_garbage_nop([](be::Garbage)
                         {
                         });

auto build_scrub_tree_nop([](const ObjectId&,
                             const vd::SnapshotName&) -> ScrubManager::ClonePtrList
                          {
                              return ScrubManager::ClonePtrList();
                          });
}

class ScrubManagerTest
    : public RegistryTestSetup
{
protected:
    ScrubManagerTest()
        : RegistryTestSetup("ScrubManagerTest")
        , cluster_id_("cluster")
        , node_id_("node")
    {}

    virtual void
    SetUp() override
    {
        RegistryTestSetup::SetUp();
        object_registry_ = std::make_unique<ObjectRegistry>(cluster_id_,
                                                            node_id_,
                                                            registry_);
    }

    virtual void
    TearDown() override
    {
        object_registry_ = nullptr;
        RegistryTestSetup::TearDown();
    }

    void
    run_scrub_manager(ScrubManager& m) const
    {
        m.work_();
    }

    using GetObjectRegistryFun = std::function<ObjectRegistry&()>;

    void
    make_clone_tree(GetObjectRegistryFun get_object_registry,
                    const ObjectId& parent,
                    const vd::SnapshotName& snap,
                    const size_t depth,
                    const size_t nodes_per_level,
                    ScrubManager::ClonePtrList& l)
    {
        yt::SourceOfUncertainty sou;

        if (depth > 0)
        {
            for (size_t i = 0; i < nodes_per_level; ++i)
            {
                const std::string s(yt::UUID().str());
                const ObjectId oid(s);

                get_object_registry().register_clone(oid,
                                                     be::Namespace(s),
                                                     parent,
                                                     snap);

                l.emplace_back(boost::make_shared<ScrubManager::Clone>(oid));
                make_clone_tree(get_object_registry,
                                oid,
                                vd::SnapshotName(s),
                                depth - 1,
                                nodes_per_level,
                                l.back()->clones);
            }
        }
    }

    ScrubManager::Clone
    make_clone_tree(const size_t depth,
                    const size_t nodes_per_level,
                    GetObjectRegistryFun get_object_registry)
    {
        const std::string s(yt::UUID().str());
        const ObjectId parent_id(s);
        const vd::SnapshotName snap(s);

        get_object_registry().register_base_volume(parent_id,
                                                   be::Namespace(s));

        ScrubManager::Clone parent(parent_id);

        make_clone_tree(get_object_registry,
                        parent_id,
                        snap,
                        depth,
                        nodes_per_level,
                        parent.clones);

        return parent;
    }

    ClusterId cluster_id_;
    NodeId node_id_;
    std::unique_ptr<ObjectRegistry> object_registry_;
};

TEST_F(ScrubManagerTest, hygiene)
{
    const ara::value_list init_keys(registry_->prefix(cluster_id_.str()));
    EXPECT_NE(0,
              init_keys.size());

    std::atomic<uint64_t> period_secs(std::numeric_limits<uint64_t>::max());
    ScrubManager mgr(*object_registry_,
                     std::static_pointer_cast<yt::LockedArakoon>(registry_),
                     period_secs,
                     apply_scrub_reply_nop,
                     build_scrub_tree_nop,
                     collect_garbage_nop);

    {
        const ara::value_list keys(registry_->prefix(cluster_id_.str()));
        EXPECT_NE(0,
                  keys.size());
        EXPECT_LE(init_keys.size(),
                  keys.size());
    }

    mgr.destroy();

    {
        const ara::value_list keys(registry_->prefix(cluster_id_.str()));
        EXPECT_NE(0,
                  keys.size());
        EXPECT_EQ(init_keys.size(),
                  keys.size());

        auto it1 = init_keys.begin();
        auto it2 = keys.begin();

        ara::arakoon_buffer buf1;
        ara::arakoon_buffer buf2;

        while (it1.next(buf1) and it2.next(buf2))
        {
            ASSERT_EQ(buf1.first,
                      buf2.first);

            EXPECT_EQ(0,
                      memcmp(buf1.second,
                             buf2.second,
                             buf1.first));
        }
    }
}

TEST_F(ScrubManagerTest, queue_work)
{
    std::atomic<uint64_t> period_secs(std::numeric_limits<uint64_t>::max());

    ScrubManager mgr(*object_registry_,
                     std::static_pointer_cast<yt::LockedArakoon>(registry_),
                     period_secs,
                     apply_scrub_reply_nop,
                     build_scrub_tree_nop,
                     collect_garbage_nop);

    const size_t num_jobs = 7;

    ScrubManager::ParentScrubs scrubs;

    for (size_t i = 0; i < num_jobs; ++i)
    {
        const yt::UUID uuid;
        const std::string s(uuid.str());
        const ObjectId oid(s);

        // g++ (4.9.2) is having issues with this
        //
        // scrubbing::ScrubReply reply(be::Namespace(s),
        //                             vd::SnapshotName(s),
        //                             s);
        //
        //         /home/arne/Projects/openvstorage/dev/volumedriver-core/src/filesystem/test/ScrubManagerTest.cpp:230:55: error: conflicting declaration ‘volumedriver::SnapshotName s’
        //                                      vd::SnapshotName(s),
        //                                                        ^
        // /home/arne/Projects/openvstorage/dev/volumedriver-core/src/filesystem/test/ScrubManagerTest.cpp:229:52: note: previous declaration as ‘backend::Namespace s’
        //          scrubbing::ScrubReply reply(be::Namespace(s),
        //
        const be::Namespace nspace(s);
        const vd::SnapshotName snap(s);

        scrubbing::ScrubReply reply(nspace,
                                    snap,
                                    s);

        scrubs.emplace(std::make_pair(reply,
                                      oid));

        mgr.queue_scrub_reply(oid,
                              reply);
    }

    EXPECT_TRUE(scrubs == mgr.get_parent_scrubs());
    EXPECT_TRUE(mgr.get_clone_scrubs().empty());

    for (const auto& p : scrubs)
    {
        EXPECT_TRUE(mgr.get_scrub_tree(p.first).empty());
    }
}

TEST_F(ScrubManagerTest, requeue_work)
{
    std::atomic<uint64_t> period_secs(std::numeric_limits<uint64_t>::max());

    ScrubManager mgr(*object_registry_,
                     std::static_pointer_cast<yt::LockedArakoon>(registry_),
                     period_secs,
                     apply_scrub_reply_nop,
                     build_scrub_tree_nop,
                     collect_garbage_nop);

    const std::string s(yt::UUID().str());
    const ObjectId oid(s);

    // cf. ScrubManagerTest.queue_work for an explanation of this verbosity
    const be::Namespace nspace(s);
    const vd::SnapshotName snap(s);
    const scrubbing::ScrubReply reply(nspace,
                                      snap,
                                      s);

    mgr.queue_scrub_reply(oid,
                          reply);

    mgr.queue_scrub_reply(oid,
                          reply);

    auto check([&]
               {
                   const ScrubManager::ParentScrubs scrubs(mgr.get_parent_scrubs());
                   ASSERT_EQ(1,
                             scrubs.size());
                   auto it = scrubs.find(reply);
                   ASSERT_TRUE(it != scrubs.end());
                   ASSERT_EQ(oid,
                             it->second);
               });

    check();

    EXPECT_THROW(mgr.queue_scrub_reply(ObjectId(yt::UUID().str()),
                                       reply),
                 ScrubManager::Exception);

    check();
}

TEST_F(ScrubManagerTest, object_id_and_namespace_mismatch)
{
    std::atomic<uint64_t> period_secs(std::numeric_limits<uint64_t>::max());

    ScrubManager mgr(*object_registry_,
                     std::static_pointer_cast<yt::LockedArakoon>(registry_),
                     period_secs,
                     apply_scrub_reply_nop,
                     build_scrub_tree_nop,
                     collect_garbage_nop);

    const std::string s(yt::UUID().str());
    const ObjectId oid(s);
    const scrubbing::ScrubReply reply(be::Namespace(yt::UUID().str()),
                                      vd::SnapshotName(s),
                                      s);

    EXPECT_THROW(mgr.queue_scrub_reply(ObjectId(yt::UUID().str()),
                                       reply),
                 ScrubManager::Exception);

    EXPECT_TRUE(mgr.get_parent_scrubs().empty());
}

TEST_F(ScrubManagerTest, parent_gone)
{
    std::atomic<uint64_t> period_secs(std::numeric_limits<uint64_t>::max());

    ScrubManager mgr(*object_registry_,
                     std::static_pointer_cast<yt::LockedArakoon>(registry_),
                     period_secs,
                     apply_scrub_reply_nop,
                     build_scrub_tree_nop,
                     [&](be::Garbage)
                     {
                         FAIL() << "there should not be any garbage";
                     });

    const std::string s(yt::UUID().str());
    const ObjectId oid(s);

    // cf. ScrubManagerTest.queue_work for an explanation of this verbosity
    const be::Namespace nspace(s);
    const vd::SnapshotName snap(s);
    const scrubbing::ScrubReply reply(nspace,
                                      snap,
                                      s);

    mgr.queue_scrub_reply(oid,
                          reply);

    run_scrub_manager(mgr);

    EXPECT_EQ(0,
              mgr.get_counters().parent_scrubs_ok);
    EXPECT_EQ(1,
              mgr.get_counters().parent_scrubs_nok);
    EXPECT_EQ(0,
              mgr.get_counters().clone_scrubs_ok);
    EXPECT_EQ(0,
              mgr.get_counters().clone_scrubs_nok);

    EXPECT_TRUE(mgr.get_parent_scrubs().empty());
}

TEST_F(ScrubManagerTest, clone_gone)
{
    const size_t num_clones = 7;
    const ScrubManager::Clone parent(make_clone_tree(1,
                                                     num_clones,
                                                     [&]() -> ObjectRegistry&
                                                     {
                                                         return *object_registry_;
                                                     }));

    ASSERT_EQ(num_clones, parent.clones.size());
    object_registry_->unregister(parent.clones.front()->id);

    const std::string id(parent.id.str());

    // cf. ScrubManagerTest.queue_work for an explanation of this verbosity
    const be::Namespace nspace(id);
    const vd::SnapshotName snap(id);
    const scrubbing::ScrubReply scrub_reply(nspace,
                                            snap,
                                            id);

    std::atomic<uint64_t> period_secs(std::numeric_limits<uint64_t>::max());

    size_t count = 0;
    bool garbage_collected = false;

    ScrubManager mgr(*object_registry_,
                     std::static_pointer_cast<yt::LockedArakoon>(registry_),
                     period_secs,
                     [&](const ObjectId& oid,
                         const scrubbing::ScrubReply& reply,
                         const volumedriver::ScrubbingCleanup cleanup)
                     -> ScrubManager::MaybeGarbage
                     {
                         EXPECT_EQ(scrub_reply,
                                   reply);

                         if (count++)
                         {
                             EXPECT_NE(oid,
                                       parent.id);
                             EXPECT_EQ(vd::ScrubbingCleanup::Never,
                                       cleanup);
                             return boost::none;
                         }
                         else
                         {
                             EXPECT_EQ(oid,
                                       parent.id);
                             EXPECT_EQ(vd::ScrubbingCleanup::OnError,
                                       cleanup);
                             return be::Garbage(be::Namespace(id),
                                                { id });
                         }
                     },
                     [&](const ObjectId& oid,
                         const vd::SnapshotName& snap) -> ScrubManager::ClonePtrList
                     {
                         EXPECT_EQ(oid,
                                   parent.id);
                         EXPECT_EQ(vd::SnapshotName(id),
                                   snap);

                         return parent.clones;
                     },
                     [&](be::Garbage garbage)
                     {
                         EXPECT_FALSE(garbage_collected);
                         garbage_collected = true;
                         EXPECT_EQ(be::Namespace(id),
                                   garbage.nspace);
                         EXPECT_TRUE(be::Garbage::ObjectNames{ id } == garbage.object_names);
                     });

    mgr.queue_scrub_reply(parent.id,
                          scrub_reply);

    while (not garbage_collected)
    {
        run_scrub_manager(mgr);
    }

    EXPECT_TRUE(mgr.get_clone_scrubs().empty());
    EXPECT_TRUE(mgr.get_scrub_tree(scrub_reply).empty());

    EXPECT_EQ(num_clones,
              count);

    EXPECT_EQ(1,
              mgr.get_counters().parent_scrubs_ok);
    EXPECT_EQ(0,
              mgr.get_counters().parent_scrubs_nok);
    EXPECT_EQ(num_clones - 1,
              mgr.get_counters().clone_scrubs_ok);
    EXPECT_EQ(1,
              mgr.get_counters().clone_scrubs_nok);

}

TEST_F(ScrubManagerTest, failure_to_apply_to_parent)
{
    std::string s;
    const size_t num_clones = 2;
    const ScrubManager::Clone parent(make_clone_tree(1,
                                                     num_clones,
                                                     [&]() -> ObjectRegistry&
                                                     {
                                                         return *object_registry_;
                                                     }));

    ASSERT_EQ(num_clones,
              parent.clones.size());

    size_t count = 0;

    const std::string id(parent.id.str());

    // cf. ScrubManagerTest.queue_work for an explanation of this verbosity
    const be::Namespace nspace(id);
    const vd::SnapshotName snap(id);
    const scrubbing::ScrubReply scrub_reply(nspace,
                                            snap,
                                            id);

    std::atomic<uint64_t> period_secs(std::numeric_limits<uint64_t>::max());

    ScrubManager mgr(*object_registry_,
                     std::static_pointer_cast<yt::LockedArakoon>(registry_),
                     period_secs,
                     [&](const ObjectId& oid,
                         const scrubbing::ScrubReply& reply,
                         const volumedriver::ScrubbingCleanup /* cleanup */)
                     -> ScrubManager::MaybeGarbage
                     {
                         EXPECT_EQ(0,
                                   count);
                         EXPECT_EQ(scrub_reply,
                                   reply);
                         EXPECT_EQ(oid,
                                   parent.id);
                         ++count;

                         throw std::runtime_error("some exception");
                     },
                     [&](const ObjectId& oid,
                         const vd::SnapshotName& snap) -> ScrubManager::ClonePtrList
                     {
                         EXPECT_EQ(oid,
                                   parent.id);
                         EXPECT_EQ(vd::SnapshotName(id),
                                   snap);

                         return parent.clones;
                     },
                     collect_garbage_nop);

    mgr.queue_scrub_reply(parent.id,
                          scrub_reply);

    ASSERT_FALSE(mgr.get_parent_scrubs().empty());

    run_scrub_manager(mgr);

    EXPECT_TRUE(mgr.get_parent_scrubs().empty());
    EXPECT_TRUE(mgr.get_clone_scrubs().empty());
    EXPECT_TRUE(mgr.get_scrub_tree(scrub_reply).empty());

    EXPECT_EQ(1,
              count);

    EXPECT_EQ(0,
              mgr.get_counters().parent_scrubs_ok);
    EXPECT_EQ(1,
              mgr.get_counters().parent_scrubs_nok);
    EXPECT_EQ(0,
              mgr.get_counters().clone_scrubs_ok);
    EXPECT_EQ(0,
              mgr.get_counters().clone_scrubs_nok);
}

TEST_F(ScrubManagerTest, random_stress)
{
    yt::SourceOfUncertainty sou;

    const size_t num_nodes = sou(1, 5);
    const size_t tree_depth = sou(4);
    const size_t clones_per_level = sou(1, 5);

    std::cout << "cluster nodes: " << num_nodes <<
        ", tree depth: " << tree_depth <<
        ", clones per level: " << clones_per_level <<
        std::endl;

    std::vector<std::unique_ptr<ObjectRegistry>> registries;
    registries.reserve(num_nodes);

    for (size_t i = 0; i < num_nodes; ++i)
    {
        const NodeId n("node-"s + boost::lexical_cast<std::string>(i));
        auto o(std::make_unique<ObjectRegistry>(cluster_id_,
                                                n,
                                                std::static_pointer_cast<yt::LockedArakoon>(registry_)));
        registries.emplace_back(std::move(o));
    }

    ASSERT_FALSE(registries.empty());

    const ScrubManager::Clone
        parent(make_clone_tree(sou(4),
                               sou(1, 5),
                               [&]() -> ObjectRegistry&
                               {
                                   return *registries[sou(num_nodes - 1)];
                               }));

    const std::string id(parent.id.str());

    const std::list<ObjectId> regs(object_registry_->list());
    ASSERT_FALSE(regs.empty());

    std::set<ObjectId> oids(regs.begin(),
                            regs.end());
    boost::mutex oids_lock;

    std::atomic<uint64_t> period_secs(std::numeric_limits<uint64_t>::max());

    std::atomic<bool> parent_scrubbed(false);
    std::atomic<bool> garbage_collected(false);
    std::atomic<size_t> count(0);
    std::atomic<size_t> errors(0);

    // cf. ScrubManagerTest.queue_work for an explanation of this verbosity
    const be::Namespace nspace(id);
    const vd::SnapshotName snap(id);
    const scrubbing::ScrubReply scrub_reply(nspace,
                                            snap,
                                            id);

    auto apply_scrub_reply([&](const ObjectId& oid,
                               const scrubbing::ScrubReply& reply,
                               const volumedriver::ScrubbingCleanup cleanup)
                           -> ScrubManager::MaybeGarbage
                           {
                               EXPECT_EQ(scrub_reply,
                                         reply);

                               if (count++ and not sou(0, 4))
                               {
                                   errors++;
                                   throw std::runtime_error("some exception");
                               }

                               {
                                   boost::lock_guard<decltype(oids_lock)>
                                       g(oids_lock);
                                   EXPECT_NE(0,
                                             oids.erase(oid));
                               }

                               if (oid == parent.id)
                               {
                                   EXPECT_EQ(vd::ScrubbingCleanup::OnError,
                                             cleanup);
                                   EXPECT_FALSE(parent_scrubbed);
                                   parent_scrubbed = true;
                                   return be::Garbage(be::Namespace(id),
                                                      { id });
                               }
                               else
                               {
                                   EXPECT_EQ(vd::ScrubbingCleanup::Never,
                                             cleanup);
                                   EXPECT_TRUE(parent_scrubbed);
                                   return boost::none;
                               }
                           });

    auto build_scrub_tree([&](const ObjectId& oid,
                              const vd::SnapshotName& snap) -> ScrubManager::ClonePtrList
                          {
                              EXPECT_EQ(ObjectId(id),
                                        oid);
                              EXPECT_EQ(vd::SnapshotName(id),
                                        snap);
                              return parent.clones;
                          });

    auto collect_garbage([&](be::Garbage garbage)
                         {
                             EXPECT_FALSE(garbage_collected) <<
                                 "there should be only one GC invocation";
                             garbage_collected = true;
                             EXPECT_EQ(be::Namespace(id),
                                       garbage.nspace);

                             EXPECT_TRUE(be::Garbage::ObjectNames{ id } == garbage.object_names);
                         });

    std::vector<std::unique_ptr<ScrubManager>> managers;
    managers.reserve(num_nodes);

    std::vector<std::future<void>> futures;
    futures.reserve(num_nodes);

    for (size_t i = 0; i < num_nodes; ++i)
    {
        auto m(std::make_unique<ScrubManager>(*registries[i],
                                              std::static_pointer_cast<yt::LockedArakoon>(registry_),
                                              period_secs,
                                              apply_scrub_reply,
                                              build_scrub_tree,
                                              collect_garbage));
        managers.emplace_back(std::move(m));
        futures.emplace_back(std::async(std::launch::async,
                                        [&, i]
                                        {
                                            while (not garbage_collected)
                                            {
                                                run_scrub_manager(*managers[i]);
                                            }
                                        }));
    }

    ASSERT_FALSE(managers.empty());

    managers.front()->queue_scrub_reply(parent.id,
                                        scrub_reply);

    for (auto& f : futures)
    {
        f.wait();
    }

    EXPECT_LE(regs.size(),
              count);
    EXPECT_TRUE(oids.empty());

    size_t parent_scrubs_ok = 0;
    size_t parent_scrubs_nok = 0;
    size_t clone_scrubs_ok = 0;
    size_t clone_scrubs_nok = 0;

    for (const auto& m : managers)
    {
        ScrubManager::Counters c(m->get_counters());

        parent_scrubs_ok += c.parent_scrubs_ok;
        parent_scrubs_nok += c.parent_scrubs_nok;
        clone_scrubs_ok += c.clone_scrubs_ok;
        clone_scrubs_nok += c.clone_scrubs_nok;
    }

    EXPECT_EQ(1,
              parent_scrubs_ok);
    EXPECT_EQ(0,
              parent_scrubs_nok);
    EXPECT_EQ(regs.size() - 1,
              clone_scrubs_ok);
        EXPECT_EQ(errors,
              clone_scrubs_nok);
}

}
