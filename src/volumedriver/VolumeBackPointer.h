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

#ifndef VOLUME_BACKPOINTER_H
#define VOLUME_BACKPOINTER_H
#include <youtils/Logging.h>
#include <youtils/IOException.h>
#include "VolumeInterface.h"
#include <youtils/Logger.h>

namespace volumedriver
{

class Volume;
class VolumeBackPointer
{
public:
    VolumeBackPointer(youtils::Logger::logger_type& logger,
                      VolumeInterface* vol = 0)
        : logger_(logger)
        , vol_(vol)
    {}
    void
    setVolume(VolumeInterface* vol)
    {
        if(not vol_)
        {
            vol_ = vol;
        }
        else
        {
            LOG_ERROR("Volume already set");
            throw fungi::IOException("Volume already set");
        }

    }

    inline VolumeInterface*
    getVolume()
    {
        if(not vol_)
        {

            LOG_ERROR("Volume not set");
            throw fungi::IOException("Volume not set");
        }
        else
        {
            return vol_;
        }
    }

    inline const VolumeInterface*
    getVolume() const
    {
        if(not vol_)
        {
            LOG_ERROR("Volume not set");
            throw fungi::IOException("Volume not set");
        }
        else
        {
            return vol_;
        }
    }

    youtils::Logger::logger_type&
    getLogger__() const
    {
        return logger_;
    }

private:
    youtils::Logger::logger_type& logger_;
    VolumeInterface* vol_;
};

}
#endif // VOLUME_BACKPOINTER_H

// Local Variables: **
// End: **
