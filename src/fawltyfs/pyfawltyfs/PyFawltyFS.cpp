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

#include "../DirectIORule.h"
#include "../ErrorRules.h"
#include "../Exception.h"
#include "../FileSystem.h"
#include "../FileSystemCall.h"

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/python/class.hpp>
#include <boost/python/def.hpp>
#include <boost/python/dict.hpp>
#include <boost/python/extract.hpp>
#include <boost/python/list.hpp>
#include <boost/python/module.hpp>
#include <boost/python/return_arg.hpp>
#include <boost/python/stl_iterator.hpp>
#include <boost/python/enum.hpp>

#include <youtils/Assert.h>
#include <youtils/Logger.h>
#include <youtils/LoggerToolCut.h>
#include <youtils/LoggingToolCut.h>
#include <youtils/PythonBuildInfo.h>

namespace bpy = boost::python;

namespace pyfawltyfs
{

namespace ffs = fawltyfs;

namespace
{
const std::string rule_id_key("rule_id");
const std::string path_regex_key("path_regex");
const std::string calls_key("calls");
const std::string ramp_up_key("ramp_up");
const std::string duration_key("duration");
const std::string delay_usecs_key("delay_usecs");
const std::string error_code_key("error_code");
const std::string num_matches_key("num_matches");
const std::string is_expired_key("is_expired");
const std::string direct_io_key("direct_io");
}

class PyFawltyFS
{
public:
    explicit PyFawltyFS(const std::string& basedir,
                        const std::string& mntpoint,
                        const bpy::list& fuse_argl)
    {
        bpy::stl_input_iterator<std::string> begin(fuse_argl);
        bpy::stl_input_iterator<std::string> end;
        fs_.reset(new ffs::FileSystem(basedir,
                                      mntpoint,
                                      std::vector<std::string>(begin, end)));
    }

    ~PyFawltyFS() = default;

    PyFawltyFS(const PyFawltyFS&) = delete;

    PyFawltyFS&
    operator=(const PyFawltyFS&) = delete;

    void
    mount()
    {
        ASSERT(fs_);
        fs_->mount();
    }

    void
    umount()
    {
        ASSERT(fs_);
        fs_->umount();
    }

    bool
    mounted() const
    {
        ASSERT(fs_);
        return fs_->mounted();
    }

    void
    add_delay_rule(uint32_t rule_id,
                   const std::string& path_regex,
                   //                   const std::set<fawltyfs::FileSystemCall>& calls,
                   const bpy::object& calls,
                   uint32_t ramp_up,
                   uint32_t duration,
                   uint32_t delay_us)
    {

        ffs::DelayRulePtr r(new ffs::DelayRule(rule_id,
                                               path_regex,
                                               calls_from_calls(calls),
                                               ramp_up,
                                               duration,
                                               delay_us));
        fs_->addDelayRule(r);
    }

    void
    remove_delay_rule(uint32_t rule_id)
    {
        fs_->removeDelayRule(rule_id);
    }

    bpy::list
    list_delay_rules()
    {
        bpy::list l;
        ffs::delay_rules_list_t rules;
        fs_->listDelayRules(rules);

        BOOST_FOREACH(auto r, rules)
        {
            bpy::dict d;
            d[rule_id_key] = r->id;
            d[path_regex_key] = r->path_regex.str();
            bpy::list calls;
            for(fawltyfs::FileSystemCall fs_call : r->calls_)
            {
                calls.append(fs_call);
            }
            d[calls_key] = calls;
            d[ramp_up_key] = r->ramp_up;
            d[duration_key] = r->duration;
            d[delay_usecs_key] = r->delay_usecs;
            d[num_matches_key] = r->matches();
            d[is_expired_key] = r->expired();
            l.append(d);
        }

        return l;
    }

    void
    add_failure_rule(uint32_t rule_id,
                     const std::string& path_regex,
                     const bpy::list calls,
                     uint32_t ramp_up,
                     uint32_t duration,
                     int error_code)
    {
        ffs::FailureRulePtr r(new ffs::FailureRule(rule_id,
                                                   path_regex,
                                                   calls_from_calls(calls),
                                                   ramp_up,
                                                   duration,
                                                   error_code));
        fs_->addFailureRule(r);
    }

    bpy::list
    list_failure_rules()
    {
        bpy::list l;
        ffs::failure_rules_list_t rules;
        fs_->listFailureRules(rules);

        BOOST_FOREACH(auto r, rules)
        {
            bpy::dict d;
            d[rule_id_key] = r->id;
            d[path_regex_key] = r->path_regex.str();
            bpy::list calls;
            for(fawltyfs::FileSystemCall fs_call : r->calls_)
            {
                calls.append(fs_call);
            }
            d[calls_key] = calls;
            d[ramp_up_key] = r->ramp_up;
            d[duration_key] = r->duration;
            d[error_code_key] = r->errcode;
            d[num_matches_key] = r->matches();
            d[is_expired_key] = r->expired();
            l.append(d);
        }

        return l;
    }

    void
    remove_failure_rule(uint32_t rule_id)
    {
        fs_->removeFailureRule(rule_id);
    }

    void
    add_direct_io_rule(uint32_t rule_id,
                      const std::string& path_regex,
                      bool direct_io)
    {
        ffs::DirectIORulePtr r(new ffs::DirectIORule(rule_id,
                                                     path_regex,
                                                     direct_io));
        fs_->addDirectIORule(r);
    }

    bpy::list
    list_direct_io_rules()
    {
        bpy::list l;
        ffs::direct_io_rules_list_t rules;
        fs_->listDirectIORules(rules);

        BOOST_FOREACH(auto r, rules)
        {
            bpy::dict d;
            d[rule_id_key] = r->id;
            d[path_regex_key] = r->path_regex.str();
            d[direct_io_key] = r->direct_io;
            l.append(d);
        }

        return l;
    }

    void
    remove_direct_io_rule(uint32_t rule_id)
    {
        fs_->removeDirectIORule(rule_id);
    }

    void
    enter()
    {
        mount();
    }


    void
    exit(bpy::object& /* exc_type */,
         bpy::object& /* exc_value */,
         bpy::object& /* traceback */)
    {
        // yuck! without this sleep, the umount throws an exception claiming the fs is
        // still busy.
        boost::this_thread::sleep(boost::posix_time::seconds(2));
        umount();
    }

private:
    DECLARE_LOGGER("PyFawltyFS");
    std::unique_ptr<ffs::FileSystem> fs_;
    std::unique_ptr<std::thread> thread_;

    std::set<fawltyfs::FileSystemCall>
    calls_from_calls(const bpy::object& calls)
    {
        typedef bpy::stl_input_iterator<fawltyfs::FileSystemCall> it_t;
        return std::set<fawltyfs::FileSystemCall>(it_t(calls), it_t());
    }

};

}

// XXX: document params
BOOST_PYTHON_MODULE(PyFawltyFS)
{

    youtils::Logger::disableLogging();
#include <youtils/LoggerToolCut.incl>

    bpy::enum_<fawltyfs::FileSystemCall>("FileSystemCall",
                                         "Type of calls on the filesystem.")
        .value("Getattr", fawltyfs::FileSystemCall::Getattr)
        .value("Access", fawltyfs::FileSystemCall::Access)
        .value("Readlink", fawltyfs::FileSystemCall::Readlink)
        .value("Readdir", fawltyfs::FileSystemCall::Readdir)
        .value("Mknod", fawltyfs::FileSystemCall::Mknod)
        .value("Mkdir", fawltyfs::FileSystemCall::Mkdir)
        .value("Unlink", fawltyfs::FileSystemCall::Unlink)
        .value("Rmdir", fawltyfs::FileSystemCall::Rmdir)
        .value("Symlink", fawltyfs::FileSystemCall::Symlink)
        .value("Rename", fawltyfs::FileSystemCall::Rename)
        .value("Link", fawltyfs::FileSystemCall::Link)
        .value("Chown", fawltyfs::FileSystemCall::Chown)
        .value("Chmod", fawltyfs::FileSystemCall::Chmod)
        .value("Truncate", fawltyfs::FileSystemCall::Truncate)
        .value("Open", fawltyfs::FileSystemCall::Open)
        .value("Read", fawltyfs::FileSystemCall::Read)
        .value("Write", fawltyfs::FileSystemCall::Write)
        .value("Statfs", fawltyfs::FileSystemCall::Statfs)
        .value("Utimens", fawltyfs::FileSystemCall::Utimens)
        .value("Release", fawltyfs::FileSystemCall::Release)
        .value("Fsync", fawltyfs::FileSystemCall::Fsync)
        .value("Opendir", fawltyfs::FileSystemCall::Opendir)
        .value("Releasedir", fawltyfs::FileSystemCall::Releasedir);



    bpy::class_<pyfawltyfs::PyFawltyFS,
                boost::noncopyable>("PyFawltyFS",
                                    "Purposefully erratic file system",
                                    bpy::init<const std::string&,
                                              const std::string&,
                                              const bpy::list&>
                                    ("Initialize a fawltyfs\n"
                                     "@param sourcedir, source directory\n"
                                     "@param mntpoint, mountpoint\n"
                                     "@param fuse_argl, list of fuse specific args (strings)"))
        .def("mount",
             &pyfawltyfs::PyFawltyFS::mount,
             "mount the filesystem\n"
             "@returns eventually")
        .def("umount",
             &pyfawltyfs::PyFawltyFS::umount,
             "umount the filesystem\n"
             "@returns nothing")
        .def("mounted",
             &pyfawltyfs::PyFawltyFS::mounted,
             "check whether the filesystem is mounted\n",
             "@returns a boolean")
        .def("addDelayRule",
             &pyfawltyfs::PyFawltyFS::add_delay_rule,
             (bpy::args("rule_id"),
              bpy::args("path_regex"),
              bpy::args("calls"),
              bpy::args("ramp_up"),
              bpy::args("duration"),
              bpy::args("delay_usecs")),
             "add a delay rule\n"
             "@param rule_id, unsigned specifying the rules' id (used for ordering!)\n"
             "@param path_regex, string with a regex for the path\n"
             "@param call_regex, the types of calls to use this rule on, a set of of FileSystemCall\n"
             "@param ramp_up, unsigned specifying the ramp_up period (number of hits)\n"
             "@param duration, unsigned specifying the duration (number of hits)\n"
             "@param delay_us, unsigned specifying the delay (in usecs) to inject\n"
             "@returns hopefully\n")
        .def("addDirectIORule",
             &pyfawltyfs::PyFawltyFS::add_direct_io_rule,
             (bpy::args("rule_id"),
              bpy::args("path_regex"),
              bpy::args("direct_io")),
             "add a direct I/O rule\n"
             "@param rule_id, unsigned specifying the rules' id (used for ordering!)\n"
             "@param path_regex, string with a regex for the path\n"
             "@param direct_io, bool indicating whether to use Direct I/O\n"
             "@returns void\n")
        .def("addFailureRule",
             &pyfawltyfs::PyFawltyFS::add_failure_rule,
             (bpy::args("rule_id"),
              bpy::args("path_regex"),
              bpy::args("calls"),
              bpy::args("ramp_up"),
              bpy::args("duration"),
              bpy::args("error_code")),
             "add a failure rule\n"
             "@param path_regex, string with a regex for the path\n"
             "@param call_regex, the types of calls to use this rule on, a set of FileSystemCall\n"
             "@param ramp_up, unsigned specifying the ramp_up period (number of hits)\n",
             "@param duration, unsigned specifying the duration (number of hits)\n"
             "@param errcode, int specifying the error code (errno) to inject\n"
             "@returns yes\n")
        .def("removeDelayRule",
             &pyfawltyfs::PyFawltyFS::remove_delay_rule,
             (bpy::args("rule_id")),
             "remove a delay rule\n"
             "@param rule_id, unsigned that identifies the rule\n"
             "@returns zilch\n")
        .def("removeDirectIORule",
             &pyfawltyfs::PyFawltyFS::remove_direct_io_rule,
             (bpy::args("rule_id")),
             "remove a direct I/O rule\n"
             "@param rule_id, unsigned that identifies the rule\n"
             "@returns nada\n")
        .def("removeFailureRule",
             &pyfawltyfs::PyFawltyFS::remove_failure_rule,
             (bpy::args("rule_id")),
             "remove a failure rule\n"
             "@param rule_id, unsigned that identifies the rule\n"
             "@returns nothing\n")
        .def("listDelayRules",
             &pyfawltyfs::PyFawltyFS::list_delay_rules,
             "list delay rules\n"
             "@returns a list of dicts each containing details of a rule\n")
        .def("listDirectIORules",
             &pyfawltyfs::PyFawltyFS::list_direct_io_rules,
             "list delay rules\n"
             "@returns a list of dicts each containing details of a rule\n")
        .def("listFailureRules",
             &pyfawltyfs::PyFawltyFS::list_failure_rules,
             "list delay rules\n"
             "@returns a list of dicts each containing details of a rule\n")
        .def("__enter__",
             &pyfawltyfs::PyFawltyFS::enter,
             boost::python::return_self<>(),
             "with-stmt context management - entrance\n")
        .def("__exit__",
             &pyfawltyfs::PyFawltyFS::exit,
             "with-stmt context management - exit\n");

    youtils::python::BuildInfo::registerize();
}

// Local Variables: **
// compile-command: "scons -D -j 4" **
// mode: c++ **
// End: **
