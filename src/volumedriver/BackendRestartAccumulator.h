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

#ifndef VD_BACKEND_RESTART_ACCUMULATOR_H_
#define VD_BACKEND_RESTART_ACCUMULATOR_H_

#include "SnapshotName.h"
#include "SnapshotPersistor.h"
#include "Types.h"
#include "VolumeConfig.h"

#include <boost/optional.hpp>

#include <youtils/Logging.h>
#include <youtils/UUID.h>

#include <backend/BackendInterface.h>

namespace volumedriver
{

class NSIDMap;

class BackendRestartAccumulator
{
public:
    BackendRestartAccumulator(NSIDMap& nsid,
                              const boost::optional<youtils::UUID>& start_cork,
                              const boost::optional<youtils::UUID>& end_cork);

    ~BackendRestartAccumulator() = default;

    BackendRestartAccumulator(const BackendRestartAccumulator&) = delete;

    BackendRestartAccumulator&
    operator=(const BackendRestartAccumulator&) = delete;

    void
    operator()(const SnapshotPersistor& sp,
               backend::BackendInterfacePtr& bi,
               const SnapshotName& snapshot_name,
               SCOCloneID clone_id);

    static const FromOldest direction = FromOldest::T;

    // returns (start_cork, end_cork]
    const CloneTLogs&
    clone_tlogs() const;

private:
    DECLARE_LOGGER("BackendRestartAccumulator");

    CloneTLogs clone_tlogs_;
    NSIDMap& nsid_;

    const boost::optional<youtils::UUID> start_cork_;
    const boost::optional<youtils::UUID> end_cork_;
    bool start_seen_;
    bool end_seen_;
};

}

#endif //!VD_BACKEND_RESTART_ACCUMULATOR_H_
