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

#ifndef VFS_SCRUB_MANAGER_H_
#define VFS_SCRUB_MANAGER_H_

#include "ClusterId.h"
#include "Object.h"

#include <boost/thread/mutex.hpp>

#include <youtils/IOException.h>
#include <youtils/LockedArakoon.h>
#include <youtils/Logging.h>
#include <youtils/PeriodicAction.h>

#include <backend/Garbage.h>

#include <volumedriver/ScrubbingCleanup.h>
#include <volumedriver/ScrubReply.h>

namespace youtils
{
class UUID;
}

namespace volumedriver
{
class SnapshotName;
}

namespace scrubbing
{
class ScrubReply;
}

namespace volumedriverfstest
{

class ScrubManagerTest;

}

namespace volumedriverfs
{

class ObjectRegistry;

// Ideas:
// * ScrubReplies are first put on a parent_scrubs_ queue (actually a map)
//   (which is persisted to Arakoon)
// * a PeriodicAction
//   ** periodically walks over this queue and applies the scrub results to the
//      destination volume
//   ** determines on on success all children and creates CloneScrubDescriptors
//      puts them to a clone_scrubs_ queue (which is again persisted to Arakoon) and
//      removes the entry from the parent_scrubs_ queue
//   ** walks over the clone_scrubs_ queue and applies the scrub results to the
//      respective clone (removing them on success, once the CloneScrubDescriptors
//      data structure is empty garbage can be collected).
class ScrubManager
{
    friend class volumedriverfstest::ScrubManagerTest;

public:
    MAKE_EXCEPTION(Exception,
                   fungi::IOException);
    MAKE_EXCEPTION(NoSuchObjectException,
                   Exception);

    struct Clone;
    using ClonePtr = boost::shared_ptr<Clone>;
    using ClonePtrList = std::list<ClonePtr>;

    struct Clone
    {
        ObjectId id;
        ClonePtrList clones;

        explicit Clone(const ObjectId& i)
            : id(i)
        {}

        Clone() = default;

        ~Clone() = default;

        template<typename A>
        void
        serialize(A& ar,
                  const unsigned /* version */)
        {
            ar & BOOST_SERIALIZATION_NVP(id);
            ar & BOOST_SERIALIZATION_NVP(clones);
        }
    };

    using MaybeGarbage = boost::optional<backend::Garbage>;
    using ParentScrubs = std::map<scrubbing::ScrubReply,
                                  ObjectId>;

    using ApplyScrubReplyFun =
        std::function<MaybeGarbage(const ObjectId&,
                                   const scrubbing::ScrubReply&,
                                   const volumedriver::ScrubbingCleanup)>;

    using CollectGarbageFun = std::function<void(backend::Garbage)>;

    using BuildScrubTreeFun =
        std::function<ClonePtrList(const ObjectId&,
                                   const volumedriver::SnapshotName&)>;

    ScrubManager(ObjectRegistry&,
                 std::shared_ptr<youtils::LockedArakoon>,
                 const std::atomic<uint64_t>& period_secs,
                 ApplyScrubReplyFun,
                 BuildScrubTreeFun,
                 CollectGarbageFun);

    ~ScrubManager() = default;

    ScrubManager(const ScrubManager&) = delete;

    ScrubManager&
    operator=(const ScrubManager&) = delete;

    void
    queue_scrub_reply(const ObjectId&,
                      const scrubbing::ScrubReply&);

    void
    destroy();

    ParentScrubs
    get_parent_scrubs();

    std::vector<scrubbing::ScrubReply>
    get_clone_scrubs();

    ClonePtrList
    get_scrub_tree(const scrubbing::ScrubReply&);

    struct Counters
    {
        uint64_t parent_scrubs_ok = 0;
        uint64_t parent_scrubs_nok = 0;
        uint64_t clone_scrubs_ok = 0;
        uint64_t clone_scrubs_nok = 0;
    };

    Counters
    get_counters() const;

private:
    DECLARE_LOGGER("ScrubManager");

    ObjectRegistry& registry_;
    const std::string parent_scrubs_key_;
    const std::string clone_scrubs_index_key_;

    std::shared_ptr<youtils::LockedArakoon> larakoon_;

    ApplyScrubReplyFun apply_scrub_reply_;
    BuildScrubTreeFun build_scrub_tree_;
    CollectGarbageFun collect_garbage_;

    youtils::PeriodicAction periodic_action_;

    mutable boost::mutex counters_lock_;
    Counters counters_;

    void
    work_();

    std::string
    make_clone_scrub_key_(const youtils::UUID&) const;

    std::string
    make_scrub_garbage_key_(const youtils::UUID&) const;

    void
    apply_to_parent_(const ObjectId&,
                     const scrubbing::ScrubReply&);

    void
    queue_to_clones_(const ObjectId&,
                     const scrubbing::ScrubReply&,
                     backend::Garbage);

    void
    drop_parent_(const ObjectId&,
                 const scrubbing::ScrubReply&);

    void
    apply_to_clones_(const youtils::UUID&,
                     const scrubbing::ScrubReply&);

    void
    apply_to_clone_(const youtils::UUID&,
                    const ObjectId&,
                    const scrubbing::ScrubReply&);

    boost::optional<bool>
    apply_(const ObjectId&,
           const scrubbing::ScrubReply&,
           const volumedriver::ScrubbingCleanup,
           MaybeGarbage&);

    void
    drop_clone_(const youtils::UUID&,
                const ObjectId&,
                const scrubbing::ScrubReply&,
                const bool keep_children);

    void
    finalize_(const youtils::UUID&,
              const scrubbing::ScrubReply&);

    void
    collect_scrub_garbage_();
};

}

#endif // !VFS_SCRUB_MANAGER_H_
