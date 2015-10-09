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

#ifndef DELETE_SNAPSHOT_H_
#define DELETE_SNAPSHOT_H_

#include "SnapshotName.h"

#include <vector>
#include <string>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <youtils/Logging.h>
#include <youtils/GlobalLockedCallable.h>


namespace volumedriver_backup
{

namespace bpt = boost::property_tree;

class DeleteSnapshot : public youtils::GlobalLockedCallable
{
public:
    DeleteSnapshot(const boost::property_tree::ptree&,
                   const std::vector<volumedriver::SnapshotName>& snapshots);

    void
    operator()();

    std::string
    info();

    const youtils::GracePeriod&
    grace_period() const
    {
        return grace_period_;
    }

private:
    DECLARE_LOGGER("DeleteSnapshot");

    bpt::ptree configuration_ptree_;
    const bpt::ptree& target_ptree_;
    const std::vector<volumedriver::SnapshotName>& snapshots_;
    const youtils::GracePeriod grace_period_;
};

}

#endif // DELETE_SNAPSHOT_H_

// Local Variables: **
// mode: c++ **
// End: **
