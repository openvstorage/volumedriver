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

#ifndef STATUSWRITER_H_
#define STATUSWRITER_H_

#include "TLogId.h"
#include "VolManager.h"

#include <exception>
#include <iosfwd>

#include <boost/optional.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <youtils/FileUtils.h>
#include <youtils/PeriodicAction.h>
#include <youtils/SpinLock.h>
#include <youtils/Time.h>

#include <backend/BackendConnectionManager.h>

namespace volumedriver_backup
{

class StatusWriter
    : private boost::property_tree::ptree
{
    DECLARE_LOGGER("StatusWriter");

    fungi::SpinLock spin_lock;

    friend class PeriodicWriter;

public:
    StatusWriter(const boost::posix_time::time_duration& report_interval,
                 const backend::Namespace& target_namespace);

    virtual ~StatusWriter();

    void
    start(const volumedriver::WriteOnlyVolume* volume,
          const uint64_t total_size);

    void
    end_snapshot(const std::string& end_snapshot);

    void
    start_snapshot(const std::string& start_snapshot);

    void
    add_seen(const uint64_t size = 4096);

    void
    add_kept(const uint64_t size = 4096);

    void
    current_tlog(const boost::optional<volumedriver::TLogId>& tlog);

    void
    finish();

    uint64_t report_interval() const;

private:
    void
    start_date();

    void
    write();

    const std::atomic<uint64_t> report_interval_;

    std::unique_ptr<youtils::PeriodicAction> action_;
    const backend::Namespace target_namespace_;
    bool report_times;

    struct timespec start_time_;
    uint64_t seen_;
    uint64_t kept_;
    uint64_t total_size_;

    const volumedriver::WriteOnlyVolume* volume_;


};

}

#endif // STATUSWRITER_H_

// Local Variables: **
// mode: c++ **
// End: **
