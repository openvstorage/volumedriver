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
