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

#ifndef VD_BACKEND_RESTART_ACCUMULATOR_H_
#define VD_BACKEND_RESTART_ACCUMULATOR_H_

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
               const std::string& snapshot_name,
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
