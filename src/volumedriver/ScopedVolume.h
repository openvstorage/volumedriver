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

#ifndef SCOPED_VOLUME_H
#define SCOPED_VOLUME_H

#include "Api.h"
#include "WriteOnlyVolume.h"

#include <youtils/Logging.h>

namespace volumedriver
{
// TO BE REVIEWED: we wanna go through API here to remove the volume from the VolManager.
template<RemoveVolumeCompletely remove_volume_completely>
struct WriteOnlyVolumeDeleter
{
    void
    operator()(WriteOnlyVolume* vol)
    {
        if (vol == nullptr)
        {
            LOG_TRACE("Deleter called on nullptr.");
        }
        else
        {
            bool uncaught_exception = std::uncaught_exception();
            try
            {
                fungi::ScopedLock lock_(api::getManagementMutex());

                api::destroyVolume(vol,
                                   remove_volume_completely);
            }
            catch(std::exception& e)
            {
                LOG_INFO("Caught an exception cleaning up the volume: " << e.what());
                if(not uncaught_exception)
                {
                    throw;
                }

            }
            catch(...)
            {
                LOG_INFO("Caught an unkown exception cleaning up the volume: ");
                if(not uncaught_exception)
                {
                    throw;
                }
            }

        }
    }
    DECLARE_LOGGER("VolumeDeleter");

};

template<DeleteLocalData delete_local_data,
         RemoveVolumeCompletely remove_volume_completely = RemoveVolumeCompletely::F,
         DeleteVolumeNamespace delete_volume_namespace = DeleteVolumeNamespace::F,
         ForceVolumeDeletion force_volume_deletion = ForceVolumeDeletion::F>
struct VolumeDeleter
{
    void
    operator()(Volume* vol)
    {
        if (vol == nullptr)
        {
            LOG_TRACE("Deleter called on nullptr.");
        }
        else
        {
            bool uncaught_exception = std::uncaught_exception();
            try
            {
                fungi::ScopedLock lock_(api::getManagementMutex());
                api::destroyVolume(vol,
                                   delete_local_data,
                                   remove_volume_completely,
                                   delete_volume_namespace,
                                   force_volume_deletion);
            }
            catch(std::exception& e)
            {
                LOG_INFO("Caught an exception cleaning up the volume: " << e.what());
                if(not uncaught_exception)
                {
                    throw;
                }

            }
            catch(...)
            {
                LOG_INFO("Caught an unkown exception cleaning up the volume: ");
                if(not uncaught_exception)
                {
                    throw;
                }
            }

        }
    }
    DECLARE_LOGGER("VolumeDeleter");

};


typedef std::unique_ptr<WriteOnlyVolume,
                        WriteOnlyVolumeDeleter<RemoveVolumeCompletely::F> >
ScopedWriteOnlyVolumeWithLocalDeletion;

// typedef std::unique_ptr<WriteOnlyVolume,
//                          VolumeDeleter<WriteOnlyVolume, DeleteLocalData::T, 7200> >
// ScopedWriteOnlyVolumeWithoutLocalDeletion;

typedef std::unique_ptr<Volume, VolumeDeleter<DeleteLocalData::T> > ScopedVolumeWithLocalDeletion;
typedef std::unique_ptr<Volume, VolumeDeleter<DeleteLocalData::F> > ScopedVolumeWithoutLocalDeletion;
}
#endif

// Local Variables: **
// mode: c++ **
// End: **
