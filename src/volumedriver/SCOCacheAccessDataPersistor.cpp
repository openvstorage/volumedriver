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

void
SCOCacheAccessDataPersistor::operator()()
{
    LOG_PERIODIC("starting");

    VolManager* vm = VolManager::get();

    std::list<SCOAccessDataPtr> sads;
    {
        std::list<VolumeId> vols;
        // Don't remove this lock without really understanding what you're doing.!!
        fungi::ScopedLock l(vm->getLock_());
        vm->updateReadActivity();
        vm->getVolumeList(vols);

        for (const auto& vol : vols)
        {
            SharedVolumePtr v = VolManager::get()->findVolumeConst_(vol);
            LOG_PERIODIC("collecting SCO access data for volume " << vol);
                SCOAccessDataPtr sad(new SCOAccessData(v->getNamespace(),
                                                       v->readActivity()));

                try
                {
                    scocache_.fillSCOAccessData(*sad);
                    sads.push_back(std::move(sad));
                }
                catch (std::exception& e)
                {
                    LOG_ERROR("Failed to collect SCO access data for " << vol <<
                              ": " << e.what());
                    // swallow it
                }
                catch (...)
                {
                    LOG_ERROR("Failed to collect SCO access data for " << vol <<
                              ": unknown exception");
                    // swallow it
                }
        }
    }

    for (SCOAccessDataPtr& sad : sads)
    {
        try
        {
            // can this throw?
            BackendInterfacePtr bi(vm->createBackendInterface(sad->getNamespace()));
            SCOAccessDataPersistor sadp(std::move(bi));
            sadp.push(*sad);
        }
        catch (std::exception& e)
        {
            LOG_ERROR("Failed to persist SCO access data for " <<
                      sad->getNamespace() << ": " << e.what() <<
                      " - proceeding");
            // swallow it
        }
        catch (...)
        {
            LOG_ERROR("Failed to persist SCO access data for " <<
                      sad->getNamespace() << ": unknown exception" <<
                      " - proceeding");
            // swallow it
        }
    }

    LOG_PERIODIC("done");
}

}

// Local Variables: **
// mode: c++ **
// End: **
