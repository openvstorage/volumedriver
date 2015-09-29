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
            const Volume* v = VolManager::get()->findVolumeConst_(vol);
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
