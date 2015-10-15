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

#include "DirectoryEntry.h"
#include "InodeAllocator.h"

#include <youtils/LockedArakoon.h>

namespace arakoon
{

using namespace ::volumedriverfs;

template<>
struct DataBufferTraits<volumedriverfs::Inode>
{
    static size_t
    size(const Inode& inode)
    {
        return sizeof(inode);
    }

    static const void*
    data(const Inode& inode)
    {
        return &inode;
    }

    static Inode
    deserialize(void* data,
                size_t size)
    {
        THROW_UNLESS(sizeof(Inode) == size);
        Inode i(Inode(*static_cast<const Inode*>(data)));
        ::free(data);
        return i;
    }
};

}

namespace volumedriverfs
{

namespace ara = arakoon;

InodeAllocator::InodeAllocator(const ClusterId& cluster_id,
                               std::shared_ptr<yt::LockedArakoon> larakoon)
    : larakoon_(larakoon)
    , cluster_id_(cluster_id)
{
    maybe_init_();
    LOG_INFO(cluster_id_ << ": started up");
}

std::string
InodeAllocator::make_key() const
{
    return cluster_id_.str() + "/last_inode";
}

void
InodeAllocator::maybe_init_()
{
    const std::string key(make_key());

    if (not larakoon_->exists(key))
    {
        LOG_INFO(cluster_id_ << ": initializing inode allocator");

        auto initialize([&](ara::sequence& seq)
                        {
                            static const Inode init(1);
                            seq.add_assert(key, ara::None());
                            seq.add_set(key, init);
                        });

        try
        {
            larakoon_->run_sequence("initialize inode allocator",
                                    std::move(initialize));
        }
        catch (ara::error_assertion_failed&)
        {
            LOG_WARN(cluster_id_ <<
                     ": failed to initialize inode allocator - another node beat us to it?");
            THROW_UNLESS(larakoon_->exists(key));
        }
    }
}

Inode
InodeAllocator::operator()()
{
    LOG_TRACE(cluster_id_ << ": allocating inode");

    Inode inode(0);

    auto alloc([&](ara::sequence& seq)
               {
                   const std::string key(make_key());
                   inode = larakoon_->get<std::string, Inode>(key);
                   seq.add_assert(key, inode);
                   seq.add_set(key, Inode(inode + 1));
               });

    larakoon_->run_sequence("allocating inode",
                            std::move(alloc),
                            yt::RetryOnArakoonAssert::T);

    LOG_INFO("Allocated inode " << inode);
    VERIFY(inode != Inode(0));

    return inode;
}

void
InodeAllocator::destroy()
{
    LOG_INFO(cluster_id_ << ": destroying inode allocator");
    larakoon_->delete_prefix(make_key());
}

}
