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

#include "Common.h"

fsal_status_t
ovs_posix_to_fsal_attributes(const struct stat *buffstat,
                             struct attrlist *fsalattr)
{
    FSAL_CLEAR_MASK(fsalattr->mask);
    /* sanity checks */
    if (!buffstat || !fsalattr)
    {
        return fsalstat(ERR_FSAL_FAULT, 0);
    }

    FSAL_CLEAR_MASK(fsalattr->mask);

    /* Fills the output struct */
    fsalattr->type = posix2fsal_type(buffstat->st_mode);
    FSAL_SET_MASK(fsalattr->mask, ATTR_TYPE);

    fsalattr->filesize = buffstat->st_size;
    FSAL_SET_MASK(fsalattr->mask, ATTR_SIZE);

    fsalattr->fsid = posix2fsal_fsid(buffstat->st_dev);
    FSAL_SET_MASK(fsalattr->mask, ATTR_FSID);

    fsalattr->fileid = buffstat->st_ino;
    FSAL_SET_MASK(fsalattr->mask, ATTR_FILEID);

    fsalattr->mode = unix2fsal_mode(buffstat->st_mode);
    FSAL_SET_MASK(fsalattr->mask, ATTR_MODE);

    fsalattr->numlinks = buffstat->st_nlink;
    FSAL_SET_MASK(fsalattr->mask, ATTR_NUMLINKS);

    fsalattr->owner = buffstat->st_uid;
    FSAL_SET_MASK(fsalattr->mask, ATTR_OWNER);

    fsalattr->group = buffstat->st_gid;
    FSAL_SET_MASK(fsalattr->mask, ATTR_GROUP);

    fsalattr->atime = posix2fsal_time(buffstat->st_atime, 0);
    FSAL_SET_MASK(fsalattr->mask, ATTR_ATIME);

    fsalattr->ctime = posix2fsal_time(buffstat->st_ctime, 0);
    FSAL_SET_MASK(fsalattr->mask, ATTR_CTIME);

    fsalattr->mtime = posix2fsal_time(buffstat->st_mtime, 0);
    FSAL_SET_MASK(fsalattr->mask, ATTR_MTIME);

    fsalattr->chgtime =
        posix2fsal_time(buffstat->st_mtime, 0);
    fsalattr->change = fsalattr->chgtime.tv_sec;
    FSAL_SET_MASK(fsalattr->mask, ATTR_CHGTIME);

    fsalattr->spaceused = buffstat->st_blocks * S_BLKSIZE;
    FSAL_SET_MASK(fsalattr->mask, ATTR_SPACEUSED);

    fsalattr->rawdev = posix2fsal_devt(buffstat->st_rdev);
    FSAL_SET_MASK(fsalattr->mask, ATTR_RAWDEV);

    return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

/**
 * ovs_posix2fsal_error :
 * Convert POSIX error codes to FSAL error codes.
 *
 * \param posix_errorcode (input):
 *        The error code returned from POSIX.
 *
 * \return The FSAL error code associated
 *         to posix_errorcode.
 *
 */
fsal_errors_t
ovs_posix2fsal_error(int posix_errorcode)
{
    switch (posix_errorcode)
    {
    case 0:
        return ERR_FSAL_NO_ERROR;

    case EPERM:
        return ERR_FSAL_PERM;

    case ENOENT:
        return ERR_FSAL_NOENT;

    case ECONNREFUSED:
    case ECONNABORTED:
    case ECONNRESET:
    case EIO:
    case ENFILE:
    case EMFILE:
    case EPIPE:
        return ERR_FSAL_IO;

    case ENODEV:
    case ENXIO:
        return ERR_FSAL_NXIO;

    case EBADF:
        return ERR_FSAL_NOT_OPENED;

    case ENOMEM:
    case ENOLCK:
        return ERR_FSAL_NOMEM;

    case EACCES:
        return ERR_FSAL_ACCESS;

    case EFAULT:
        return ERR_FSAL_FAULT;

    case EEXIST:
        return ERR_FSAL_EXIST;

    case EXDEV:
        return ERR_FSAL_XDEV;

    case ENOTDIR:
        return ERR_FSAL_NOTDIR;

    case EISDIR:
        return ERR_FSAL_ISDIR;

    case EINVAL:
        return ERR_FSAL_INVAL;

    case EFBIG:
        return ERR_FSAL_FBIG;

    case ENOSPC:
        return ERR_FSAL_NOSPC;

    case EMLINK:
        return ERR_FSAL_MLINK;

    case EDQUOT:
        return ERR_FSAL_DQUOT;

    case ENAMETOOLONG:
        return ERR_FSAL_NAMETOOLONG;

    case ENOTEMPTY:
    case -ENOTEMPTY:
        return ERR_FSAL_NOTEMPTY;

    case ESTALE:
        return ERR_FSAL_STALE;

    case EAGAIN:
    case EBUSY:
        return ERR_FSAL_DELAY;

    case ENOTSUP:
        return ERR_FSAL_NOTSUPP;

    case EOVERFLOW:
        return ERR_FSAL_OVERFLOW;

    case EDEADLK:
        return ERR_FSAL_DEADLOCK;

    case EINTR:
        return ERR_FSAL_INTERRUPT;

    case EROFS:
        return ERR_FSAL_ROFS;

    case ESRCH:		/* Returned by quotaclt */
        return ERR_FSAL_NO_QUOTA;

    default:
        return ERR_FSAL_SERVERFAULT;
    }
}

fsal_errors_t
std_system_to_fsal_error(const std::error_code& ec)
{
    if (ec.category() == std::system_category())
    {
        return ovs_posix2fsal_error(ec.value());
    }
    else
    {
        return ERR_FSAL_SERVERFAULT;
    }
}

fsal_errors_t
boost_system_to_fsal_error(const boost::system::error_code& ec)
{
    if (ec.category() == boost::system::system_category())
    {
        return ovs_posix2fsal_error(ec.value());
    }
    else
    {
        return ERR_FSAL_SERVERFAULT;
    }
}

std::ostream&
operator<<(std::ostream& os,
           const fsal_fsid_t& id)
{
    return os << "Major: " << id.major << ", Minor: " << id.minor;
}

std::ostream&
operator<<(std::ostream& os,
           const timespec& ts)
{
    boost::io::ios_all_saver osg(os);
    return os << ts.tv_sec << "," << std::setw(9) << std::setfill('0') << ts.tv_nsec;
}

/* Needs to be outside the namespace to work... */
std::ostream&
operator<<(std::ostream& os,
           const attrlist& attrlist)
{
    os << "type: ";
    if(attrlist.mask bitand ATTR_TYPE)
    {
        os << ganesha::object_file_type(attrlist.type);
    }
    else
    {
        os << "not set";
    }
    os << std::endl;
    os << "filesize: ";
    if(attrlist.mask bitand ATTR_SIZE)
    {
        os << attrlist.filesize;
    }
    else
    {
        os << "not set" ;
    }
    os << std::endl;

    os << "fsid: ";
    if(attrlist.mask bitand ATTR_FSID)
    {
        os << attrlist.fsid;
    }
    else
    {
        os << "not set";
    }
    os << std::endl;

    os << "acl: ";

    if(attrlist.mask bitand ATTR_ACL)
    {
        os << attrlist.acl;
    }
    else
    {
        os << "not set";
    }
    os << std::endl;

    os << "fileid: ";

    if(attrlist.mask bitand ATTR_FILEID)
    {
        os << attrlist.fileid;
    }
    else
    {
        os << "not set";
    }
    os  << std::endl;

    os << "numlinks: ";

    if(attrlist.mask bitand ATTR_NUMLINKS)
    {
        os << attrlist.numlinks;
    }
    else
    {
        os << "not set";
    }
    os << std::endl;

    os << "owner: ";
    if(attrlist.mask bitand ATTR_OWNER)
    {
        os << attrlist.owner;
    }
    else
    {
        os << "not set";
    }
    os << std::endl;

    os << "group: ";

    if(attrlist.mask bitand ATTR_GROUP)
    {
        os << attrlist.group;
    }
    else
    {
        os << "not set";
    }
    os << std::endl;

    os << "rawdev: ";
    if(attrlist.mask bitand ATTR_RAWDEV)
    {
        os << "Some fsal_dev_t rawdev I can't print";
        //   os << attrlist.rawdev;
    }
    else
    {
        os << "not set";
    }
    os  << std::endl;

    os << "atime: ";
    if(attrlist.mask bitand ATTR_ATIME)
    {
        os << attrlist.atime;

    }
    else
    {
        os << "not set";
    }
    os  << std::endl;

    os << "creation: ";
    if(attrlist.mask bitand ATTR_CREATION)
    {
        os << attrlist.creation;
    }
    else
    {
        os << "not set";
    }
    os  << std::endl;

    os << "ctime: ";
    if(attrlist.mask bitand ATTR_CTIME)
    {
        os << attrlist.ctime;
    }
    else
    {
        os << "not set";
    }
    os  << std::endl;

    os << "mtime: ";
    if(attrlist.mask bitand ATTR_MTIME)
    {
        os << attrlist.mtime;
    }
    else
    {
        os << "not set";
    }
    os  << std::endl;

    os << "chgtime: ";
    if(attrlist.mask bitand ATTR_CHGTIME)
    {
        os << attrlist.chgtime;
    }
    else
    {
        os << "not set";
    }
    os  << std::endl;

    os << "spaceused: ";
    if(attrlist.mask bitand ATTR_SPACEUSED)
    {
        os << attrlist.spaceused;
    }
    else
    {
        os << "not set";
    }
    os  << std::endl;

    os << "generation: ";

    if(attrlist.mask bitand ATTR_GENERATION)
    {
        os << attrlist.generation;
    }
    else
    {
        os << "not set";
    }
    os  << std::endl;

    os << "mode: ";

    if(attrlist.mask bitand ATTR_MODE)
    {
        os << attrlist.mode;
    }
    else
    {
        os << "not set";
    }
    os  << std::endl;

#if 0
    if(attrlist.mask bitand ATTR_RDATTR_ERR)
    {
    }
         else
    {
        os << "not set";
    }
    os  << std::endl;

    if(attrlist.mask bitand ATTR_CHANGE)
    {
    }
         else
    {
        os << "not set";
    }
    os  << std::endl;

    if(attrlist.mask bitand ATTR_ATIME_SERVER)
    {
    }
    else
    {
        os << "not set";
    }
    os  << std::endl;

    if(attrlist.mask bitand ATTR_MTIME_SERVER)
    {
    }
         else
    {
        os << "not set";
    }
    os  << std::endl;
#endif
    return os;
}

std::ostream&
operator<<(std::ostream& os,
           const fsal_lock_param_t& in)
{
    os << "lock_sle_type: ";

    switch (in.lock_sle_type)
    {
    case FSAL_POSIX_LOCK:
        os << "FSAL_POSIX_LOCK";
        break;
    case FSAL_LEASE_LOCK:
        os << "FSAL_LEASE_LOCK";
        break;
    default:
        os << "Unknown ?? ";
    }
    os << std::endl;

    os << "lock_type: " << ganesha::lockType(in.lock_type) << std::endl;
    os << "lock_start: " <<  in.lock_start << std::endl;
    os << "lock_length: " << in.lock_length << std::endl;

    return os;
}

namespace ganesha
{

LockOp
lockOp(fsal_lock_op_t in)
{
    switch(in)
    {
    case FSAL_OP_LOCKT:
        return LockOp::Try;
    case FSAL_OP_LOCK:
        return LockOp::NonBlocking;
    case FSAL_OP_LOCKB:
        return LockOp::Blocking;
    case FSAL_OP_UNLOCK:
        return LockOp::Unlock;
    case FSAL_OP_CANCEL:
        return LockOp::Cancel;
    default:
        UNREACHABLE;
    }
}

LockType
lockType(fsal_lock_t in)
{
    switch(in)
    {
    case FSAL_LOCK_R:
        return LockType::Read;
    case FSAL_LOCK_W:
        return LockType::Write;
    case FSAL_NO_LOCK:
        return LockType::NoLock;
    default:
        UNREACHABLE;
    }
}

ObjectFileType
object_file_type(const object_file_type_t in)
{
    switch(in)
    {
    case NO_FILE_TYPE:
        return ObjectFileType::NO_FILE_TYPE;
    case REGULAR_FILE:
        return ObjectFileType::REGULAR_FILE;
    case CHARACTER_FILE:
        return ObjectFileType::CHARACTER_FILE;
    case BLOCK_FILE:
        return ObjectFileType::BLOCK_FILE;
    case SYMBOLIC_LINK:
        return ObjectFileType::SYMBOLIC_LINK;
    case SOCKET_FILE:
        return ObjectFileType::SOCKET_FILE;
    case FIFO_FILE:
        return ObjectFileType::FIFO_FILE;
    case DIRECTORY:
        return ObjectFileType::DIRECTORY;
    case EXTENDED_ATTR:
        return ObjectFileType::EXTENDED_ATTR;
    default:
        throw 1;
    }
}

FileSystemWrapper*
get_wrapper(struct fsal_export *exp)
{
    return container_of(exp, struct this_fsal_export, export_)->wrapper_;
}

FileSystemWrapper*
get_wrapper(struct fsal_obj_handle *hdl)
{
    this_fsal_obj_handle *handle_obj =
        container_of(hdl, struct this_fsal_obj_handle, obj_handle_);
    return get_wrapper(handle_obj->export_);
}

FileSystemWrapper&
get_fsw(struct fsal_obj_handle *obj_hdl)
{
    return *get_wrapper(obj_hdl);
}

FileSystemWrapper&
get_fsw(struct fsal_export *exp_hdl)
{
    return *get_wrapper(exp_hdl);
}

mode_t
mode(const struct attrlist* attrlist)
{
    VERIFY(attrlist->mask bitand ATTR_MODE);
    return attrlist->mode;
}

uint64_t
owner(const struct attrlist* attrlist)
{
    VERIFY(attrlist->mask bitand ATTR_OWNER);
    return attrlist->owner;
}

volumedriverfs::ObjectId
create_object_id(unsigned char id[])
{
    return volumedriverfs::ObjectId(std::string(id, id + UUID_LENGTH));
}

void
create_handle_id(struct this_fsal_obj_handle *hdl,
                 volumedriverfs::ObjectId id)
{
    memcpy(hdl->id_, id.c_str(), UUID_LENGTH);
    hdl->id_[UUID_LENGTH] = '\0';
}

struct this_fsal_obj_handle*
alloc_handle(struct fsal_export* exp_hdl,
             volumedriverfs::Inode inode,
             const struct stat *st)
{
    TRACE_("struct fsal_export* exp_hdl: " << exp_hdl << std::endl
           << "vfs::Inode inode: " << inode << std::endl);

    auto *hdl =
        (this_fsal_obj_handle*) gsh_calloc(1,
                                           sizeof(struct this_fsal_obj_handle));
    if(not hdl)
    {
        return nullptr;
    }

    hdl->obj_handle_.attributes.mask = exp_hdl->ops->fs_supported_attrs(exp_hdl);
    ovs_posix_to_fsal_attributes(st, &hdl->obj_handle_.attributes);
    hdl->inode_ = inode;
    hdl->export_ = exp_hdl;
    hdl->fsdata.handle_ = nullptr;
    fsal_obj_handle_init(&hdl->obj_handle_, exp_hdl,
                         hdl->obj_handle_.attributes.type);
    return hdl;
}

std::ostream&
operator<<(std::ostream& os,
           const this_fsal_obj_handle& hdl)
{
    return os << "this_fsal_obj_handle*: " << &hdl << ", "
              << "fsal_obj_handle: " << &hdl.obj_handle_ << ", "
              << "inode_: " << hdl.inode_;
}

} //namespace ganesha
