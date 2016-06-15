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

#include "ClusterId.h"
#include "Object.h"
#include "ObjectRegistry.h"
#include "ScrubTreeBuilder.h"

#include <volumedriver/Api.h>
#include <volumedriver/SnapshotName.h>

namespace volumedriverfs
{

namespace vd = volumedriver;

namespace
{

void
collect_clones(ObjectRegistry& registry,
               const ObjectId& parent_id,
               ScrubManager::ClonePtrList& clones)
{
    using Entry = std::tuple<const ObjectId*, ScrubManager::ClonePtrList*>;
    using Stack = std::deque<Entry>;

    Stack stack{ std::make_tuple(&parent_id,
                                 &clones) };

    while (not stack.empty())
    {
        const ObjectId* oid = nullptr;
        ScrubManager::ClonePtrList* l = nullptr;
        std::tie(oid, l) = stack.back();
        stack.pop_back();

        const ObjectRegistrationPtr reg(registry.find(*oid));
        if (reg)
        {
            for (const auto& p :reg->treeconfig.descendants)
            {
                auto c(boost::make_shared<ScrubManager::Clone>(p.first));
                l->emplace_back(c);
                stack.push_back(std::make_tuple(&c->id,
                                                &c->clones));
            }
        }
    }
}

}

ScrubTreeBuilder::ScrubTreeBuilder(ObjectRegistry& registry)
    : registry_(registry)
{}

ScrubManager::ClonePtrList
ScrubTreeBuilder::operator()(const ObjectId& parent_id,
                             const vd::SnapshotName& snap)
{
    std::list<vd::SnapshotName> snapl;
    {
        fungi::ScopedLock g(api::getManagementMutex());
        api::showSnapshots(static_cast<const vd::VolumeId&>(parent_id.str()),
                           snapl);
    }

    while (not snapl.empty())
    {
        if (snapl.front() != snap)
        {
            snapl.pop_front();
        }
        else
        {
            break;
        }
    }

    using SnapshotSet = std::set<vd::SnapshotName>;

    SnapshotSet snaps(snapl.begin(),
                      snapl.end());

    ScrubManager::ClonePtrList clones;

    if (not snaps.empty())
    {
        ObjectRegistrationPtr reg(registry_.find(parent_id));
        if (reg)
        {
            for (const auto& p : reg->treeconfig.descendants)
            {
                if (p.second and snaps.find(*p.second) == snaps.end())
                {
                    LOG_INFO(parent_id << ": *not* adding (" << p.first <<
                             ", " << p.second <<
                             ") to scrub tree as its snapshot is not the same as / younger than " <<
                             snap);
                }
                else
                {
                    LOG_INFO(parent_id << ": adding (" << p.first <<
                             ", " << p.second <<
                             ") to scrub tree as its snapshot is the same as / younger than " <<
                             snap);

                    auto c(boost::make_shared<ScrubManager::Clone>(p.first));
                    clones.emplace_back(c);
                    collect_clones(registry_,
                                   c->id,
                                   c->clones);
                }
            }
        }
    }

    return clones;
}

}
