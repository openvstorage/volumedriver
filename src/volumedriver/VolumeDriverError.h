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

#ifndef VOLUME_DRIVER_ERROR_H_
#define VOLUME_DRIVER_ERROR_H_

#include "Events.pb.h"
#include "Types.h"
#include "VolumeDriverEvents.pb.h"
#include "VolumeFailOverState.h"

#include <boost/optional.hpp>

#include <youtils/Logging.h>

namespace volumedriver
{

struct VolumeDriverError
{
    DECLARE_LOGGER("VolumeDriverError");

    static void
    report(const VolumeId& volid,
           VolumeFailOverState old_state,
           VolumeFailOverState new_state) noexcept;

    static void
    report(const events::Event& ev) noexcept;

    static void
    report(events::VolumeDriverErrorCode code,
           const std::string& desc,
           const boost::optional<VolumeId>& volume = boost::none) noexcept;
};

}

#endif // VOLUME_DRIVER_ERROR_H

// Local Variables: **
// mode: c++ **
// End: **
