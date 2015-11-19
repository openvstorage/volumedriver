// Copyright 2015 iNuron NV
//
// licensed under the Apache license, Version 2.0 (the "license");
// you may not use this file except in compliance with the license.
// You may obtain a copy of the license at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the license is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the license for the specific language governing permissions and
// limitations under the license.

#include "ClusterId.h"
#include "ObjectRegistry.h"
#include "ScrubManager.h"

#include <map>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/shared_ptr.hpp>

#include <youtils/Serialization.h>

#include <backend/Garbage.h>

#include <volumedriver/ScrubReply.h>

namespace volumedriverfs
{

namespace ara = arakoon;
namespace be = backend;
namespace vd = volumedriver;
namespace yt = youtils;

using namespace std::literals::string_literals;

#define LOCK_COUNTERS()                         \
    boost::lock_guard<decltype(counters_lock_)> clg__(counters_lock_)

namespace
{

template<typename A>
void
serialize(A& ar,
          ScrubManager::Clone& c,
          const unsigned /* version */)
{
    ar & boost::serialization::make_nvp("id",
                                        c.id);
    ar & boost::serialization::make_nvp("clones",
                                        c.clones);
};

using CloneScrubs = std::map<scrubbing::ScrubReply,
                             youtils::UUID>;

using PerNodeGarbage = std::list<youtils::UUID>;

using IArchive = boost::archive::text_iarchive;
using OArchive = boost::archive::text_oarchive;

using namespace std::literals::string_literals;

DECLARE_LOGGER("ScrubManagerUtils");

template<typename T>
T
deserialize(const ara::buffer& buf)
{
    return buf.as_istream<T>([](std::istream& is) -> T
        {
            T t;
            IArchive ia(is);
            ia >> t;
            return t;
        });
}

template<typename T>
std::string
serialize(const T& t)
{
    std::ostringstream os;
    OArchive oa(os);
    oa << t;
    return os.str();
}

std::string
scrub_manager_prefix(const ClusterId& cluster_id)
{
    return "scrubmgr/"s + cluster_id.str();
}

std::string
parent_scrubs_key(const ClusterId& cluster_id)
{
    return scrub_manager_prefix(cluster_id) + "/parents"s;
}

std::string
clone_scrubs_prefix(const ClusterId& cluster_id)
{
    return scrub_manager_prefix(cluster_id) + "/clones"s;
}

std::string
garbage_prefix(const ClusterId& cluster_id)
{
    return scrub_manager_prefix(cluster_id) + "/garbage"s;
}

std::string
clone_scrubs_index_key(const ClusterId& cluster_id)
{
    return clone_scrubs_prefix(cluster_id) + "/index";
}

std::string
node_garbage_queue_key(const ClusterId& cluster_id,
                       const NodeId& node_id)
{
    return scrub_manager_prefix(cluster_id) +
        "/nodes/"s +
        node_id.str() + "/garbage"s;
}

template<typename T>
void
maybe_init_key(yt::LockedArakoon& larakoon,
               const std::string& key)
{
    if (not larakoon.exists(key))
    {
        LOG_INFO("Initializing " << key);

        try
        {
            larakoon.run_sequence(("initializing "s + key).c_str(),
                                  [&](ara::sequence& seq)
                                  {
                                      seq.add_assert(key,
                                                     ara::None());
                                      seq.add_set(key,
                                                  serialize(T()));
                                  });
        }
        catch (ara::error_assertion_failed&)
        {
            LOG_INFO("Failed to initialize " << key <<
                     " - someone else beat us to it");
        }
    }
    else
    {
        LOG_INFO(key << " already initialized");
    }
}

}

ScrubManager::ScrubManager(ObjectRegistry& registry,
                           std::shared_ptr<yt::LockedArakoon> larakoon,
                           const std::atomic<uint64_t>& period_secs,
                           ApplyScrubReplyFun apply_scrub_reply,
                           BuildScrubTreeFun build_scrub_tree,
                           CollectGarbageFun collect_garbage)
    : registry_(registry)
    , parent_scrubs_key_(parent_scrubs_key(registry_.cluster_id()))
    , clone_scrubs_index_key_(clone_scrubs_index_key(registry_.cluster_id()))
    , larakoon_(larakoon)
    , apply_scrub_reply_(std::move(apply_scrub_reply))
    , build_scrub_tree_(std::move(build_scrub_tree))
    , collect_garbage_(std::move(collect_garbage))
    , periodic_action_("ScrubManager",
                       boost::bind(&ScrubManager::work_,
                                   this),
                       period_secs)
{
    maybe_init_key<ParentScrubs>(*larakoon_,
                                 parent_scrubs_key_);
    maybe_init_key<CloneScrubs>(*larakoon_,
                                clone_scrubs_index_key_);
    maybe_init_key<PerNodeGarbage>(*larakoon_,
                                   node_garbage_queue_key(registry_.cluster_id(),
                                                         registry_.node_id()));
}

void
ScrubManager::destroy()
{
    larakoon_->delete_prefix(scrub_manager_prefix(registry_.cluster_id()));
}

void
ScrubManager::queue_scrub_reply(const ObjectId& oid,
                                const scrubbing::ScrubReply& scrub_reply)
{
    auto fun([&](ara::sequence& seq)
             {
                 const ara::buffer parent_buf(larakoon_->get(parent_scrubs_key_));
                 auto parent_scrubs(deserialize<ParentScrubs>(parent_buf));
                 auto it = parent_scrubs.find(scrub_reply);
                 if (it != parent_scrubs.end())
                 {
                     if (it->second == oid)
                     {
                         LOG_INFO(oid << ": " << scrub_reply <<
                                  " already on parent queue!");
                         return;
                     }
                     else
                     {
                         LOG_ERROR("Attempt to queue " << scrub_reply <<
                                   " for " << oid <<
                                   " but it's already queued for " << it->second);
                         throw Exception("Attempt to queue ScrubReply to another object");
                     }
                 }
                 else if (oid.str() != scrub_reply.ns_.str())
                 {
                     TODO("AR: reconsider this");
                     // This is a blantant violation of layering / encapsulation:
                     // the knowledge that object ID and namespace ID are the same
                     // should be kept isolated to a single place. But checking it
                     // here simply makes most sense.
                     LOG_ERROR("Scrub reply's namespace " << scrub_reply.ns_ <<
                               " and object ID " << oid << " don't match!");
                     throw Exception("Attempt to queue ScrubReply to wrong object");
                 }

                 const ara::buffer clone_buf(larakoon_->get(clone_scrubs_index_key_));
                 const auto clone_scrubs(deserialize<CloneScrubs>(clone_buf));
                 if (clone_scrubs.find(scrub_reply) != clone_scrubs.end())
                 {
                     LOG_INFO(scrub_reply << ": already on clone queue!");
                     return;
                 }

                 parent_scrubs.emplace(std::make_pair(scrub_reply,
                                                      oid));

                 seq.add_assert(parent_scrubs_key_,
                                parent_buf);

                 seq.add_set(parent_scrubs_key_,
                             serialize(parent_scrubs));
             });

    larakoon_->run_sequence("queue scrub reply",
                            std::move(fun),
                            yt::RetryOnArakoonAssert::T);
}

std::string
ScrubManager::make_clone_scrub_key_(const yt::UUID& uuid) const
{
    return clone_scrubs_prefix(registry_.cluster_id()) + uuid.str();
}

std::string
ScrubManager::make_scrub_garbage_key_(const yt::UUID& uuid) const
{
    return garbage_prefix(registry_.cluster_id()) + uuid.str();
}

ScrubManager::ParentScrubs
ScrubManager::get_parent_scrubs()
{
    return deserialize<ParentScrubs>(larakoon_->get(parent_scrubs_key_));
}

std::vector<scrubbing::ScrubReply>
ScrubManager::get_clone_scrubs()
{
    auto clone_scrubs(deserialize<CloneScrubs>(larakoon_->get(clone_scrubs_index_key_)));
    std::vector<scrubbing::ScrubReply> res;
    res.reserve(clone_scrubs.size());

    for (auto&& cs : clone_scrubs)
    {
        res.emplace_back(std::move(cs.first));
    }

    return res;
}

ScrubManager::ClonePtrList
ScrubManager::get_scrub_tree(const scrubbing::ScrubReply& reply)
{
    ClonePtrList l;

    const auto clone_scrubs(deserialize<CloneScrubs>(larakoon_->get(clone_scrubs_index_key_)));
    auto it = clone_scrubs.find(reply);
    if (it != clone_scrubs.end())
    {
        try
        {
            l = deserialize<ClonePtrList>(larakoon_->get(make_clone_scrub_key_(it->second)));
        }
        catch (ara::error_not_found&)
        {
            LOG_INFO(reply << ": removed while trying to retrieve?");
        }
    }

    return l;
}

ScrubManager::Counters
ScrubManager::get_counters() const
{
    LOCK_COUNTERS();
    return counters_;
}

void
ScrubManager::work_()
{
    LOG_INFO("inspecting parent scrub queue");

    const auto parent_scrubs(get_parent_scrubs());

    for (const auto& p : parent_scrubs)
    {
        try
        {
            apply_to_parent_(p.second,
                             p.first);
        }
        CATCH_STD_ALL_LOG_IGNORE("Failed to apply " << p.first <<
                                 " to parent " << p.second);
    }

    LOG_INFO("inspecting clone scrub queue");

    const auto
        clone_scrubs(deserialize<CloneScrubs>(larakoon_->get(clone_scrubs_index_key_)));

    for (const auto& p : clone_scrubs)
    {
        try
        {
            apply_to_clones_(p.second,
                             p.first);
        }
        CATCH_STD_ALL_LOG_IGNORE("Failed to apply " << p.first << " to clones");
    }

    try
    {
        collect_scrub_garbage_();
    }
    CATCH_STD_ALL_LOG_IGNORE("Failed to collect garbage");
}

boost::optional<bool>
ScrubManager::apply_(const ObjectId& oid,
                     const scrubbing::ScrubReply& reply,
                     const vd::ScrubbingCleanup cleanup,
                     MaybeGarbage& maybe_garbage)
{
    // This could be simplified - there's no need to check the registration here,
    // we could instead rely on exceptions entirely. They do have a performance
    // impact though so let's play nice and avoid them as much as possible.
    const ObjectRegistrationPtr reg(registry_.find(oid));
    if (not reg)
    {
        LOG_INFO(oid << ": not registered (anymore?)");
        return boost::none;
    }

    if (reg->node_id == registry_.node_id())
    {
        LOG_INFO(oid << ": registered locally, applying scrub reply");
        maybe_garbage = apply_scrub_reply_(oid,
                                           reply,
                                           cleanup);
        return true;
    }
    else
    {
        LOG_INFO(oid << ": not registered here but on " << reg->node_id);
        return false;
    }
}

void
ScrubManager::apply_to_parent_(const ObjectId& oid,
                               const scrubbing::ScrubReply& reply)
{
    LOG_INFO(reply << ": checking " << oid);

    MaybeGarbage maybe_garbage;
    boost::optional<bool> res;

    try
    {
        res = apply_(oid,
                     reply,
                     vd::ScrubbingCleanup::OnError,
                     maybe_garbage);
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR(oid << ": failed to apply " << reply << ": " << EWHAT <<
                      ". Dropping it.");
        });

    if (not res)
    {
        TODO("AR: plug storage leak");
        {
            LOCK_COUNTERS();
            ++counters_.parent_scrubs_nok;
        }
        drop_parent_(oid,
                     reply);
    }
    else if (*res)
    {
        VERIFY(maybe_garbage);

        {
            LOCK_COUNTERS();
            ++counters_.parent_scrubs_ok;
        }

        queue_to_clones_(oid,
                         reply,
                         std::move(*maybe_garbage));
    }
}

void
ScrubManager::drop_parent_(const ObjectId& oid,
                           const scrubbing::ScrubReply& scrub_reply)
{
    LOG_INFO(oid << ": dropping " << scrub_reply);

    auto fun([&](ara::sequence& seq)
             {
                 const ara::buffer buf(larakoon_->get(parent_scrubs_key_));
                 auto scrubs(deserialize<ParentScrubs>(buf));

                 if (scrubs.erase(scrub_reply))
                 {
                     seq.add_assert(parent_scrubs_key_,
                                    buf);
                     seq.add_set(parent_scrubs_key_,
                                 serialize(scrubs));
                 }
             });

    larakoon_->run_sequence("dropping ScrubReply",
                            std::move(fun),
                            yt::RetryOnArakoonAssert::T);
}

void
ScrubManager::queue_to_clones_(const ObjectId& oid,
                               const scrubbing::ScrubReply& scrub_reply,
                               be::Garbage garbage)
{
    LOG_INFO(scrub_reply << ": queueing it to clones of " << oid);

    const ClonePtrList clones(build_scrub_tree_(oid,
                                                scrub_reply.snapshot_name_));

    auto fun([&](ara::sequence& seq)
             {
                 const ara::buffer parent_buf(larakoon_->get(parent_scrubs_key_));
                 auto parent_scrubs(deserialize<ParentScrubs>(parent_buf));

                 VERIFY(parent_scrubs.erase(scrub_reply));

                 const ara::buffer clone_buf(larakoon_->get(clone_scrubs_index_key_));
                 auto clone_scrubs(deserialize<CloneScrubs>(clone_buf));

                 const yt::UUID uuid;
                 const auto res(clone_scrubs.emplace(std::make_pair(scrub_reply,
                                                                    uuid)));
                 VERIFY(res.second);

                 seq.add_assert(parent_scrubs_key_,
                                parent_buf);

                 seq.add_assert(clone_scrubs_index_key_,
                                clone_buf);

                 seq.add_set(make_scrub_garbage_key_(uuid),
                             serialize(garbage));

                 seq.add_set(make_clone_scrub_key_(uuid),
                             serialize(clones));

                 seq.add_set(clone_scrubs_index_key_,
                             serialize(clone_scrubs));

                 seq.add_set(parent_scrubs_key_,
                             serialize(parent_scrubs));

                 LOG_INFO(scrub_reply << ": moving from parent " << oid <<
                          " to clones, UUID " << uuid);
             });

    larakoon_->run_sequence("moving scrub reply from parent to clone queue",
                            std::move(fun),
                            yt::RetryOnArakoonAssert::T);
}

void
ScrubManager::apply_to_clones_(const yt::UUID& uuid,
                               const scrubbing::ScrubReply& reply)
{
    LOG_INFO(reply << ": applying to clones, UUID " << uuid);

    ClonePtrList clones;

    try
    {
        clones = deserialize<ClonePtrList>(larakoon_->get(make_clone_scrub_key_(uuid)));
    }
    catch (ara::error_not_found&)
    {
        LOG_INFO(reply <<
                 ": no more clones found, someone else must have cleaned up already");
    }

    for (const auto& c : clones)
    {
        apply_to_clone_(uuid,
                        c->id,
                        reply);
    }

    if (clones.empty())
    {
        finalize_(uuid,
                  reply);
    }
}

void
ScrubManager::drop_clone_(const yt::UUID& uuid,
                          const ObjectId& oid,
                          const scrubbing::ScrubReply& reply,
                          bool promote_children)
{
    LOG_INFO(reply << ": removing clone " << oid << " from queue, UUID " <<
             uuid << ", promoting children: " << promote_children);

    auto fun([&](ara::sequence& seq)
             {
                 const std::string key(make_clone_scrub_key_(uuid));
                 ara::buffer buf(larakoon_->get(key));

                 auto clones(deserialize<ClonePtrList>(buf));

                 bool found = false;
                 ClonePtrList children;

                 for (auto it = clones.begin();
                      it != clones.end();
                      ++it)
                 {
                     ClonePtr c(*it);
                     if (c->id == oid)
                     {
                         children = c->clones;
                         clones.erase(it);
                         found = true;
                         break;
                     }
                 }

                 if (found)
                 {
                     if (promote_children)
                     {
                         clones.splice(clones.end(),
                                       children);
                     }

                     seq.add_assert(key,
                                    buf);

                     seq.add_set(key,
                                 serialize(clones));
                 }
             });

    try
    {
        larakoon_->run_sequence("erase inexistent clone from queue",
                                std::move(fun),
                                yt::RetryOnArakoonAssert::T);
    }
    catch (ara::error_not_found&)
    {
        LOG_INFO(reply << ", UUID " << uuid <<
                 ": no more clones found, someone else must have cleaned up already");
    }
}

void
ScrubManager::apply_to_clone_(const yt::UUID& uuid,
                              const ObjectId& oid,
                              const scrubbing::ScrubReply& reply)
{
    LOG_INFO(reply << ": applying to clone " << oid << ", UUID " << uuid);

    MaybeGarbage maybe_garbage;
    boost::optional<bool> res;
    try
    {
         res = apply_(oid,
                      reply,
                      vd::ScrubbingCleanup::Never,
                      maybe_garbage);
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR(oid << ": failed to apply " << reply << ", UUID " <<
                      uuid << ": " << EWHAT);
            LOCK_COUNTERS();
            ++counters_.clone_scrubs_nok;
            throw;
        });

    if (not res)
    {
        LOG_INFO(oid << ": failed to apply " << reply << ", UUID " << uuid <<
                 ": not present anymore, dropping it");

        LOCK_COUNTERS();
        ++counters_.clone_scrubs_nok;

        drop_clone_(uuid,
                    oid,
                    reply,
                    false);
    }
    else if (*res)
    {
        VERIFY(not maybe_garbage);

        {
            LOCK_COUNTERS();
            ++counters_.clone_scrubs_ok;
        }

        LOG_INFO(oid << ": successfully applied " << reply << ", UUID " << uuid <<
                 " - dropping it");

        drop_clone_(uuid,
                    oid,
                    reply,
                    true);
    }
}

void
ScrubManager::finalize_(const yt::UUID& uuid,
                        const scrubbing::ScrubReply& reply)
{
    LOG_INFO(reply << ", UUID " << uuid << ": all done, cleaning up");

    const std::string garbage_key(node_garbage_queue_key(registry_.cluster_id(),
                                                         registry_.node_id()));
    const std::string clone_scrub_key(make_clone_scrub_key_(uuid));
    auto fun([&](ara::sequence& seq)
             {
                 const ara::buffer cbuf(larakoon_->get(clone_scrubs_index_key_));
                 auto scrubs(deserialize<CloneScrubs>(cbuf));

                 if (scrubs.erase(reply))
                 {
                     seq.add_assert(clone_scrubs_index_key_,
                                    cbuf);
                     seq.add_set(clone_scrubs_index_key_,
                                 serialize(scrubs));

                     const ara::buffer lbuf(larakoon_->get(garbage_key));
                     auto garbage_list(deserialize<PerNodeGarbage>(lbuf));
                     garbage_list.emplace_back(uuid);

                     seq.add_assert(garbage_key,
                                    lbuf);
                     seq.add_set(garbage_key,
                                 serialize(garbage_list));

                     seq.add_delete(clone_scrub_key);
                 }
             });
    try
    {
        larakoon_->run_sequence("finalize scrub reply",
                                std::move(fun),
                                yt::RetryOnArakoonAssert::T);
    }
    catch (ara::error_not_found&)
    {
        LOG_INFO(reply << ", UUID " << uuid <<
                 ": someone else collected our garbage!?");
    }
}

void
ScrubManager::collect_scrub_garbage_()
{
    LOG_INFO("collecting garbage");

    const std::string key(node_garbage_queue_key(registry_.cluster_id(),
                                                 registry_.node_id()));

    auto fun([&](ara::sequence& seq)
             {
                 const ara::buffer lbuf(larakoon_->get(key));
                 auto l(deserialize<PerNodeGarbage>(lbuf));
                 seq.add_assert(key,
                                lbuf);

                 for (const auto& uuid : l)
                 {
                     auto g(deserialize<be::Garbage>(larakoon_->get(make_scrub_garbage_key_(uuid))));
                     collect_garbage_(std::move(g));
                 }

                 l.clear();

                 seq.add_set(key,
                             serialize(l));
             });

    larakoon_->run_sequence("collect garbage",
                            std::move(fun),
                            yt::RetryOnArakoonAssert::F);
}

}
