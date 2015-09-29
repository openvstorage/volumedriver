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
