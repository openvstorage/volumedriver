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

#include "GaneshaTestSetup.h"

#include <sys/types.h>
#include <signal.h>

#include <boost/filesystem/fstream.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/scope_exit.hpp>
#include <boost/thread.hpp>

#include <simple-nfs/Mount.h>

namespace ganeshatest
{

namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace vfst = volumedriverfstest;

fs::path Setup::ganesha_path_;
fs::path Setup::fsal_path_;
boost::optional<uint16_t> Setup::gdbserver_port_;

Setup::Setup(const vfst::FileSystemTestSetupParameters& params)
    : vfst::FileSystemTestSetup(params)
    , child_pid_(-1)
{}

std::string
Setup::nfs_server() const
{
    return "127.0.0.1"; // "localhost" does not (necessarily?) work w/ libnfs.
}

fs::path
Setup::nfs_export() const
{
    return fs::path("/") / fs::path(params_.name_);
}

void
Setup::configure_fs()
{
    bpt::ptree pt;
    make_config_(pt, topdir_, local_node_id());
    bpt::json_parser::write_json(volumedriverfs_config_file(topdir_).string(),
                                 pt);

    put_cluster_node_configs_in_registry(pt);
}

void
Setup::configure_ganesha() const
{
    const fs::path path(ganesha_config_file(topdir_));
    fs::remove_all(path);

    fs::ofstream ofs(path);

    ASSERT_TRUE(ofs.good());

    ofs << "FSAL\n"
        "{\n"
        "\tOVS\n"
        "\t{\n"
        //"\t\tFSAL_Shared_Library = " << fsal_path_ << ";\n"
        "\t\tDebugLevel = \"NIV_CRIT\";\n"
        "\t\tLogFile = " << topdir_ / "ganesha.log" << ";\n"
        "\t\tmax_FS_calls = 0;\n"
        "\t}\n"
        "}\n"
        "FileSystem\n"
        "{\n"
        "\t\tUmask = 0000;\n"
        "}\n"
        "FILESYSTEM\n"
        "{\n"
        "\tOpenByHandleDeviceFile = \"/dev/openhandle_dev\";\n"
        "}\n"
        "CacheInode { Use_Getattr_Directory_Invalidation = 1; }\n"
        "NFS_Core_Param\n"
        "\t{\n"
        "\tNb_Worker = 10;\n"
        "\tNFS_Port = " << nfs_port() << ";\n"
        "\tMNT_Port = 0;\n"
        "\tNLM_Port = 0;\n"
        "\tRQUOTA_Port = 0;\n"
        "\tPlugins_Dir = " << fsal_path_ << ";\n"
        "\t}\n"
        "EXPORT\n"
        "\t{\n"
        "\tExport_Id = 77;\n"
        "\tPath = " << nfs_export() << ";\n"
        "\tFSAL {\n"
        "\t\tname = OVS;\n"
        "\t\tovs_config = " << volumedriverfs_config_file(topdir_) << ";\n"
        "\t\tovs_discovery_port = 21212;\n"
        "\t\t}\n"
        "\tAccess_Type = RW;\n"
        "\tPseudo = \"/ganesha-vfs\";\n"
        "\tFilesystem_id = 192.168;\n"
        "\tPrivilegedPort = FALSE;\n"
        "\tProtocols = \"3,4\";\n"
        "\tTransports = \"TCP\";\n"
        "\tTag = \"filesystem\";\n"
        "\tSecType = \"sys\";\n"
        "\t}\n" << std::endl;

    ofs.flush();
    ASSERT_TRUE(ofs.good());
}

void
Setup::wait_for_ganesha_()
{
    const uint32_t retry_secs = 60;
    const uint32_t sleep_msecs = gdbserver_port_ ? 5000 : 250;

    for (uint32_t i = 0; i < retry_secs * 1000 / sleep_msecs; ++i)
    {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(sleep_msecs));

        LOG_TRACE("Checking whether we can access the ganesha share at " <<
                  nfs_server() << ", port " << nfs_port() << ", export " <<
                  nfs_export());
        try
        {
            simple_nfs::Mount m(nfs_server(), nfs_export());
            LOG_INFO("ganesha share " << m.share_name() << " successfully mounted");
            return;
        }
        catch (simple_nfs::Exception& e)
        {
        }
    }

    LOG_TRACE("Failed to access the ganesha share at " <<
              nfs_server() << ", port " << nfs_port() << ", export " <<
              nfs_export() << " within " << retry_secs << " seconds");
}

void
Setup::start_ganesha()
{
    ASSERT_TRUE(!fs::exists(ganesha_pid_file(topdir_)));
    ASSERT_TRUE(fs::exists(ganesha_config_file(topdir_)));

    std::vector<char*> args;
    args.reserve(20); // just a guess to prevent too many reallocs

    BOOST_SCOPE_EXIT((&args))
    {
        for (auto a : args)
        {
            free(a);
        }
    }
    BOOST_SCOPE_EXIT_END;

#define CARG(cstr)                          \
    args.push_back(::strdup(cstr))

#define SARG(str)                           \
    CARG((str).c_str())

#define PARG(path)                          \
    SARG((path).string())

    if (gdbserver_port_)
    {
        CARG("gdbserver");
        SARG(std::string(":") + boost::lexical_cast<std::string>(*gdbserver_port_));
    }

    PARG(ganesha_path_);

    CARG("-p");
    PARG(ganesha_pid_file(topdir_));
    CARG("-f");
    PARG(ganesha_config_file(topdir_));
    CARG("-L");
    PARG(topdir_ / "ganesha.log");
    CARG("-N");
    CARG("NIV_EVENT");

#undef CARG
#undef SARG
#undef PARG

    args.push_back(nullptr);

    pid_t pid = ::fork();
    ASSERT_LE(0, pid);

    if (pid == 0)
    {
        // child
        int ret = 0;
        if (gdbserver_port_)
        {
            ret = execvp("gdbserver",
                         args.data());
        }
        else
        {
            ret = ::execv(ganesha_path_.c_str(),
                          args.data());
        }
        ASSERT_EQ(0, ret);
        exit(0);
    }

    ASSERT_LT(0, pid);
    child_pid_ = pid;

    wait_for_ganesha_();
}

boost::optional<pid_t>
Setup::ganesha_pid()
{
    pid_t pid = -1;

    {
        const fs::path pidfile(ganesha_pid_file(topdir_));
        fs::ifstream ifs(pidfile);
        if (not ifs.good())
        {
            LOG_INFO("Failed to open " << pidfile);
            return boost::none;
        }

        ifs >> pid;
        if (pid < 0)
        {
            LOG_ERROR("Failed to read pid from " << pidfile);
            return boost::none;
        }
    }

    {
        fs::path procfile("/proc");
        procfile /= boost::lexical_cast<std::string>(pid);
        procfile /= std::string("cmdline");

        fs::ifstream ifs(procfile);
        if (not ifs.good())
        {
            LOG_INFO("Failed to open " << procfile);
            return boost::none;
        }

        std::string cmdline;
        ifs >> cmdline;

        const std::string exec_path = cmdline.substr(0, cmdline.find_first_of('\0'));
        if (exec_path != ganesha_path_)
        {
            LOG_INFO("expected " << ganesha_path_ << ", but " << pid << " is running " <<
                     exec_path);
            return boost::none;
        }
    }

    LOG_INFO("ganesha's pid is very probably " << pid);
    return pid;
}

void
Setup::stop_ganesha()
{
    // The child_pid_ is not necessarily ganesha's - it could be gdbserver's.
    // So in order to shut things down more or less orderly we send a signal to ganesha
    // and wait for gdbserver to exit.
    const boost::optional<pid_t> maybe_pid(ganesha_pid());

    if (maybe_pid)
    {
        // 2 possibilities:
        // - gdbserver + ganesha still alive
        // - ganesha still alive
        ASSERT_LT(0, child_pid_);
        LOG_TRACE("Shutting down ganesha @ pid " << *maybe_pid);
        // For now we need to use SIGKILL as the orderly shutdown with SIGTERM
        // gets stuck in the shutdown path (cf. OVS-555).
        int ret = ::kill(*maybe_pid, SIGTERM);
        ASSERT_EQ(0, ret) << "failed to kill ganesha @ " << *maybe_pid << ": " <<
            strerror(errno);

        ASSERT_EQ(child_pid_, ::waitpid(child_pid_, &ret, 0));
        child_pid_ = -1;
    }
    else if (child_pid_ > 0)
    {
        // 2 possibilities:
        // - gdbserver + ganesha not alive anymore
        // - ganesha not alive anymore (-> zombie)
        LOG_WARN("We do seem to have a leftover child @ pid " <<
                 child_pid_ << " - killing it");

        int ret = ::kill(child_pid_, SIGKILL);
        ASSERT_EQ(0, ret) << "failed to kill ganesha @ pid " <<
            *maybe_pid << ": " << strerror(errno);

        ASSERT_EQ(child_pid_, ::waitpid(child_pid_, &ret, 0));
        child_pid_ = -1;
    }
    else
    {
        // forking off gdbserver / ganesha failed / never happened
        LOG_WARN("ganesha not running!?");
    }
}

void
Setup::SetUp()
{
    vfst::FileSystemTestSetup::SetUp();

    configure_fs();
    configure_ganesha();
    start_ganesha();
}

void
Setup::TearDown()
{
    stop_ganesha();

    vfst::FileSystemTestSetup::TearDown();
}

}
