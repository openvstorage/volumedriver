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

#ifndef COMMON_H
#define COMMON_H

/* we need to handle GetAttrOnInexistentPath specially */
#include <filesystem/FileSystem.h>

extern "C" {
#include <fsal.h>
#include <fsal_types.h>
#include <fsal_api.h>
#include <fsal_convert.h>
#include <FSAL/fsal_init.h>
#include <FSAL/fsal_commonlib.h>
}

#include <boost/io/ios_state.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <boost/thread/thread.hpp>

#define DONT_DEFINE_LOGGING_MACROS
#include <youtils/Catchers.h>
#include <youtils/Logging.h>
#include <youtils/Logger.h>
#include <youtils/System.h>
#include <youtils/ScopeExit.h>

#include "FileSystemWrapper.h"
#include "TCPServer.h"

/* youtils::UUID length */
#define UUID_LENGTH 36

#define OK return fsalstat(ERR_FSAL_NO_ERROR, 0)
#define NOK return fsalstat(ERR_FSAL_NOTSUPP, 0)

void export_ops_init(struct export_ops *ops);
void handle_ops_init(struct fsal_obj_ops *ops);

fsal_status_t
ovs_posix_to_fsal_attributes(const struct stat *buffstat,
                             struct attrlist *fsalattr);

fsal_errors_t
ovs_posix2fsal_error(int posix_errorcode);

fsal_errors_t
std_system_to_fsal_error(const std::error_code& ec);

fsal_errors_t
boost_system_to_fsal_error(const boost::system::error_code& ec);

std::ostream&
operator<<(std::ostream& os,
           const fsal_fsid_t& id);

std::ostream&
operator<<(std::ostream& os,
           const timespec& ts);

/* Needs to be outside the namespace to work... */
std::ostream&
operator<<(std::ostream& os,
           const attrlist& attrlist);

std::ostream&
operator<<(std::ostream& os,
           const fsal_lock_param_t& in);

namespace ganesha
{

// TODO: having this free-standing in a header still ain't a good idea despite
// the 'inline' which at least prevents multiple copies of it as it still means
// that anyone including it will use that logger by default / there's potential
// for conflicts with local loggers.
// Wrap all of this in a class and make it static members.
// In fact, some of the above functions should also be moved into it.
inline DECLARE_LOGGER("GaneshaFileSystem");
// Look what the cat ganesha dragged in: we can't use youtils/Logging.h macros
// here as these clash with syslog related names.
#define LOG_(sev, msg)                                                        \
    if (::youtils::Logger::filter(getLogger__().name, sev))                   \
    {                                                                         \
        BOOST_LOG_SEV(getLogger__().get(), sev) <<                            \
            __FUNCTION__  << "(" << __LINE__ << "): " << msg;                 \
    }

// TRACE clashes with the eponymous macro from youtils::Tracer
#define TRACE_(msg) LOG_(youtils::Severity::trace, msg)
#define DEBUG(msg) LOG_(youtils::Severity::debug, msg)
#define INFO(msg) LOG_(youtils::Severity::info, msg)
#define WARN(msg) LOG_(youtils::Severity::warning, msg)
#define ERROR(msg) LOG_(youtils::Severity::error, msg)
#define FATAL(msg) LOG_(youtils::Severity::fatal, msg)
#define NOTIFY(msg) LOG_(youtils::Severity::notification, msg)

struct this_fsal_obj_handle
{
    struct fsal_obj_handle obj_handle_;
    struct fsal_export *export_;
    fsal_openflags_t openflags;
    volumedriverfs::Inode inode_;
    unsigned char id_[UUID_LENGTH + 1];
    FSData fsdata;
};

struct this_fsal_module {
    struct fsal_module fsal;
    struct fsal_staticfsinfo_t fs_info;
};

struct this_fsal_export {
    struct fsal_export export_;
    FileSystemWrapper* wrapper_;
    ovs_discovery_server* server_;
};

// TODO: drop the fsw_ ... and use a namespace or, as suggested above, wrap
// a struct or class around it and make it a static member.
template<typename ...A>
fsal_status_t
fsw_exception_handler(FileSystemWrapper& fsw_,
                      void (FileSystemWrapper::*func)(A ...args),
                      A ...args)
{
    try
    {
        ((&fsw_)->*func)(std::forward<A>(args)...);
    }
    catch (std::system_error& e)
    {
        ERROR("Caught a std::system_error: " << e.what());
        return fsalstat(std_system_to_fsal_error(e.code()), 0);
    }
    catch (boost::system::system_error& e)
    {
        ERROR("Caught a boost::system::system_error: " << e.what());
        return fsalstat(boost_system_to_fsal_error(e.code()), 0);
    }
    catch (ganesha::FileSystemWrapperException& e)
    {
        ERROR("Caught a ganesha exception (" << e.error_ << "): " <<
              e.what());
        return fsalstat((fsal_errors_t)e.error_, 0);
    }
    catch (volumedriverfs::GetAttrOnInexistentPath& e)
    {
        TRACE_("Caught a GetAttrOnInexistentPath exception: "
               << e.what());
        return fsalstat(ERR_FSAL_NOENT, 0);
    }
    catch (volumedriverfs::HierarchicalArakoon::DoesNotExistException& e)
    {
        TRACE_("Caught a DoesNotExistException exception: "
               << e.what());
        return fsalstat(ERR_FSAL_NOENT, 0);
    }
    catch (volumedriverfs::FileExistsException& e)
    {
        TRACE_("Caught a FileExistsException exception: "
               << e.what());
        return fsalstat(ERR_FSAL_EXIST, 0);
    }
    catch (std::exception& e)
    {
        ERROR("Caught a std exception: " << e.what());
        return fsalstat(ERR_FSAL_SERVERFAULT, 0);
    }
    catch (...)
    {
        ERROR("Caught an unknown exception") ;
        return fsalstat(ERR_FSAL_SERVERFAULT, 0);
    }

    return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

LockOp
lockOp(fsal_lock_op_t in);

LockType
lockType(fsal_lock_t in);

ObjectFileType
object_file_type(const object_file_type_t in);

FileSystemWrapper*
get_wrapper(struct fsal_export *exp);

FileSystemWrapper*
get_wrapper(struct fsal_obj_handle *hdl);

FileSystemWrapper&
get_fsw(struct fsal_obj_handle *obj_hdl);

FileSystemWrapper&
get_fsw(struct fsal_export *exp_hdl);

mode_t
mode(const struct attrlist* attrlist);

uint64_t
owner(const struct attrlist* attrlist);

volumedriverfs::ObjectId
create_object_id(unsigned char id[]);

void
create_handle_id(struct this_fsal_obj_handle *hdl,
                 volumedriverfs::ObjectId id);

struct this_fsal_obj_handle*
alloc_handle(struct fsal_export* exp_hdl,
             volumedriverfs::Inode inode,
             const struct stat *st);

std::ostream&
operator<<(std::ostream& os,
           const this_fsal_obj_handle& hdl);
} //namespace ganesha

#endif
