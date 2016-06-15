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

#ifndef VOLUMEDRIVER_BACKUP_RESTORE_H_
#define VOLUMEDRIVER_BACKUP_RESTORE_H_

#include <boost/filesystem.hpp>
#include <boost/property_tree/info_parser.hpp>

#include <iosfwd>
#include <youtils/IOException.h>
#include <youtils/Logging.h>

#include <volumedriver/VolumeConfig.h>
// Y42 probably needs to go
#include <youtils/WithGlobalLock.h>
namespace backend
{
class BackendInterface;
}

namespace volumedriver_backup
{

namespace be = backend;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace vd = volumedriver;
namespace yt = youtils;

MAKE_EXCEPTION(RestoreException, fungi::IOException);
MAKE_EXCEPTION(WrongVolumeRole, RestoreException);
MAKE_EXCEPTION(FailedBackupException, RestoreException);

class Restore
{
public:
    Restore(const fs::path& configuration_file,
            bool barf_on_busted_backup,
            const youtils::GracePeriod&);

    Restore(const Restore&) = delete;

    Restore&
    operator=(const Restore&) = delete;

    ~Restore() = default;

    void
    operator()();

    static void
    print_configuration_documentation(std::ostream& os);

    const youtils::GracePeriod&
    grace_period() const
    {
        return grace_period_;
    }

    std::string
    info();

private:
    DECLARE_LOGGER("Restore");

    const fs::path configuration_file_;
    fs::path scratch_dir_;
    const bool barf_on_busted_backup_;
    void
    init_(const bpt::ptree& pt);

    void
    sanitize_snapshots_(be::BackendInterfacePtr bi) const;

    void
    promote_(vd::VolumeConfig::WanBackupVolumeRole new_role,
             be::BackendInterfacePtr bi,
             vd::VolumeConfig& cfg);

    void
    copy_(be::BackendInterfacePtr source_bi,
          be::BackendInterfacePtr target_bi);

    void
    collect_garbage_(be::BackendInterfacePtr bi,
                     const vd::OrderedTLogIds& doomed_tlogs) const;
public:
    const youtils::GracePeriod grace_period_;

};

}

#endif // VOLUMEDRIVER_BACKUP_RESTORE_H_

// Local Variables: **
// bvirtual-targets: ("bin/backup_volume_driver") **
// compile-command: "scons -D -j 4 --kernel_version=system --ignore-buildinfo" **
// mode: c++ **
// End: **
