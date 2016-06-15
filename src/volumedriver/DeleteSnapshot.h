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
