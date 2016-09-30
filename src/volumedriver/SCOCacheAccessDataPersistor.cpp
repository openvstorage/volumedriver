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

#include "SCOAccessData.h"
#include "SCOCacheAccessDataPersistor.h"
#include "VolManager.h"

namespace volumedriver
{

namespace be = backend;

void
SCOCacheAccessDataPersistor::operator()()
{
    LOG_PERIODIC("starting");

    VolManager* vm = VolManager::get();

    using SadCondition = std::pair<SCOAccessDataPtr,
                                   boost::shared_ptr<be::Condition>>;

    std::vector<SadCondition> sad_conds;

    {
        std::list<VolumeId> vols;
        // Don't remove this lock without really understanding what you're doing.!!
        fungi::ScopedLock l(vm->getLock_());
        vm->updateReadActivity();
        vm->getVolumeList(vols);

        sad_conds.reserve(vols.size());

        for (const auto& vol : vols)
        {
            SharedVolumePtr v = VolManager::get()->findVolumeConst_(vol);
            LOG_PERIODIC("collecting SCO access data for volume " << vol);
            SCOAccessDataPtr sad(new SCOAccessData(v->getNamespace(),
                                                   v->readActivity()));

            try
            {
                scocache_.fillSCOAccessData(*sad);
                sad_conds.emplace_back(std::make_pair(std::move(sad),
                                                      v->backend_write_condition()));
            }
            CATCH_STD_ALL_LOG_IGNORE("Failed to collect SCO access data for " << vol);
        }
    }

    for (const auto& sad_cond : sad_conds)
    {
        try
        {
            // can this throw?
            BackendInterfacePtr bi(vm->createBackendInterface(sad_cond.first->getNamespace()));
            SCOAccessDataPersistor sadp(std::move(bi));
            sadp.push(*sad_cond.first,
                      sad_cond.second);
        }
        CATCH_STD_ALL_LOG_IGNORE("Failed to persist SCO access data for " <<
                                 sad_cond.first->getNamespace());
    }

    LOG_PERIODIC("done");
}

}

// Local Variables: **
// mode: c++ **
// End: **
