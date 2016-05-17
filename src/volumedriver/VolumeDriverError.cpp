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

#include "VolumeDriverError.h"
#include "VolManager.h"

#include <youtils/Assert.h>
#include <youtils/Catchers.h>

namespace volumedriver
{

namespace
{

DECLARE_LOGGER("VolumeDriverErrorUtils");

events::DTLState
translate_foc_state(VolumeFailOverState s)
{
    switch (s)
    {
    case VolumeFailOverState::OK_SYNC:
        return events::DTLState::Sync;
    case VolumeFailOverState::OK_STANDALONE:
        return events::DTLState::Standalone;
    case VolumeFailOverState::KETCHUP:
        return events::DTLState::Catchup;
    case VolumeFailOverState::DEGRADED:
        return events::DTLState::Degraded;
    }

    VERIFY(0 == "forgot to add a newly introduced state?");
}

}

void
VolumeDriverError::report(const VolumeId& volid,
                          VolumeFailOverState old_state,
                          VolumeFailOverState new_state) noexcept
{
    try
    {
        events::Event ev;
        auto msg = ev.MutableExtension(events::dtl_state_transition);
        msg->set_volume_name(volid.str());
        msg->set_old_state(translate_foc_state(old_state));
        msg->set_new_state(translate_foc_state(new_state));

        report(ev);
    }
    CATCH_STD_ALL_LOG_IGNORE("Failed to report DTL state transition event, volume " <<
                             volid << ", old state " << old_state << ", new state " <<
                             new_state);

    TODO("AR: the logging could emit an exception, violating noexcept?");
}

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
