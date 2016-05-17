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
    DECLARE_LOGGER("WriteOnlyVolumeDeleter");

};

typedef std::unique_ptr<WriteOnlyVolume,
                        WriteOnlyVolumeDeleter<RemoveVolumeCompletely::F> >
ScopedWriteOnlyVolumeWithLocalDeletion;

}
#endif

// Local Variables: **
// mode: c++ **
// End: **
