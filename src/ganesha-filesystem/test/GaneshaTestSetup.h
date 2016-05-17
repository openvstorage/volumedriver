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

#ifndef VFS_GANESHA_TEST_SETUP_H_
#define VFS_GANESHA_TEST_SETUP_H_

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

#include <youtils/Logging.h>

#include <filesystem/test/FileSystemTestSetup.h>

namespace ganeshatest
{

class Setup
    : public volumedriverfstest::FileSystemTestSetup
{
public:
    static void
    set_ganesha_path(const boost::filesystem::path& p)
    {
        ganesha_path_ = p;
    }

    static void
    set_fsal_path(const boost::filesystem::path& p)
    {
        fsal_path_ = p;
    }

    static void
    set_gdbserver_port(uint16_t p)
    {
        ASSERT_LT(0, p);
        gdbserver_port_ = p;
    }

protected:
    explicit Setup(const volumedriverfstest::FileSystemTestSetupParameters& params);

    virtual void
    SetUp();

    virtual void
    TearDown();

    std::string
    nfs_server() const;

    boost::filesystem::path
    nfs_export() const;

    uint16_t
    nfs_port() const
    {
        // TODO: make configurable to allow several testers running concurrently.
        // However, we first need to figure out if libnfs can deal with it.
        return 2049;
    }

    static boost::filesystem::path
    volumedriverfs_config_file(const boost::filesystem::path& parent_dir)
    {
        return parent_dir / "volumedriverfs.json";
    }

    static boost::filesystem::path
    ganesha_config_file(const boost::filesystem::path& parent_dir)
    {
        return parent_dir / "ganesha.conf";
    }

    static boost::filesystem::path
    ganesha_pid_file(const boost::filesystem::path& parent_dir)
    {
        return parent_dir / "ganesha.pid";
    }

    void
    configure_fs();

    void
    configure_ganesha() const;

    void
    start_ganesha();

    void
    wait_for_ganesha_();

    void
    stop_ganesha();

    boost::optional<pid_t>
    ganesha_pid();

    static boost::filesystem::path ganesha_path_;
    static boost::filesystem::path fsal_path_;
    static boost::optional<uint16_t> gdbserver_port_;

    pid_t child_pid_;

private:
    DECLARE_LOGGER("GaneshaTestSetup");
};

}

#endif // !VFS_GANESHA_TEST_SETUP_H_
