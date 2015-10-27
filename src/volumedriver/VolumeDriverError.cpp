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

#include "VolumeDriverError.h"
#include "VolManager.h"

#include <youtils/Catchers.h>
#include <youtils/Assert.h>

namespace volumedriver
{

void
VolumeDriverError::report(events::VolumeDriverErrorCode code,
                          const std::string& desc,
                          const boost::optional<VolumeId>& volume) noexcept
{
    try
    {
        events::Event ev;
        auto msg = ev.MutableExtension(events::volumedriver_error);
        msg->set_code(code);
        msg->set_info(desc);
        if (volume)
        {
            msg->set_volume_name(volume->str());
        }

        report(ev);
    }
    CATCH_STD_ALL_LOG_IGNORE("Failed to report event code " << code <<
                             ", volume " << volume <<
                             ", info " << desc);

    TODO("AR: the logging could emit an exception, violating noexcept?");
}

void
VolumeDriverError::report(const events::Event& ev) noexcept
{
    try
    {
        events::PublisherPtr p(VolManager::get()->event_publisher());
        if (p)
        {
            p->publish(ev);
        }
        else
        {
            LOG_WARN("No event publisher present, discarding event");
        }
    }
    CATCH_STD_ALL_LOG_IGNORE("Failed to report event");

    TODO("AR: the logging could emit an exception, violating noexcept?");
}

}

// Local Variables: **
// mode: c++ **
// End: **
