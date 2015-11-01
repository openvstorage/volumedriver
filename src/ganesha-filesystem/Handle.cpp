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

#include "Common.h"
#include "Tracepoints_tp.h"

namespace
{

using namespace ganesha;
namespace yt = youtils;
namespace vfs = volumedriverfs;

/**
 * @brief Release an object
 *
 * Releases an object
 *
 * @param[in] obj_hdl The object to release
 *
 * @return FSAL status code
 */
static void
fsal_release_handle(struct fsal_obj_handle *obj_hdl)
{
    struct this_fsal_obj_handle *objhandle;

    objhandle = container_of(obj_hdl,
                             struct this_fsal_obj_handle,
                             obj_handle_);

    tracepoint(openvstorage_fsalovs,
               fsal_release_handle_enter,
               (const char *)objhandle->id_);

    auto on_exit(yt::make_scope_exit([&]{
                tracepoint(openvstorage_fsalovs,
                           fsal_release_handle_exit,
                           (const char *)objhandle->id_,
                           std::uncaught_exception());
                }));


    TRACE_("struct fsal_obj_handle *obj_hdl: " << obj_hdl);

    fsw_exception_handler<const vfs::ObjectId&>(get_fsw(obj_hdl),
                                                &FileSystemWrapper::release,
                                                create_object_id(objhandle->id_));
    fsal_obj_handle_uninit(&objhandle->obj_handle_);
    gsh_free(objhandle);
}

/**
 * @brief Look up an object by name
 *
 * This function looks up an object by name in a directory.
 *
 * @param[in] parent  The directory in which to look up the object
 * @param[in] path    The name to look up
 * @param[out] handle The looked up object.
 *
 * @return FSAL status codes.
 */
static fsal_status_t
fsal_lookup(struct fsal_obj_handle *parent,
            const char *path,
            struct fsal_obj_handle **handle)
{

    *handle = nullptr;
    struct stat st;
    boost::optional<vfs::ObjectId> id;
    this_fsal_obj_handle *hdl = nullptr;
    this_fsal_obj_handle *parent_hdl_obj = container_of(parent,
                                                        struct this_fsal_obj_handle,
                                                        obj_handle_);

    tracepoint(openvstorage_fsalovs,
               fsal_lookup_enter,
               (const char *)parent_hdl_obj->id_,
               path);


    TRACE_("struct fsal_obj_hdl: " << parent << std::endl
           << "const char* path: " << path << std::endl);

    fsal_status_t status =
        fsw_exception_handler<const vfs::ObjectId&,
                              const std::string&,
                              decltype((id)),
                              struct stat&>(get_fsw(parent),
                                            &FileSystemWrapper::lookup,
                                            create_object_id(parent_hdl_obj->id_),
                                            path,
                                            id,
                                            st);
    if (FSAL_IS_ERROR(status))
    {
        goto exit;
    }

    hdl = alloc_handle(parent_hdl_obj->export_,
                       vfs::Inode(st.st_ino),
                       &st);
    if (not hdl)
    {
        status.major = ERR_FSAL_NOMEM;
        goto exit;
    }

    create_handle_id(hdl,
                     *id);
    *handle = &hdl->obj_handle_;

exit:
    tracepoint(openvstorage_fsalovs,
               fsal_lookup_exit,
               (const char *)parent_hdl_obj->id_,
               path,
               (const char *)hdl->id_,
               status.major);

    return status;
}

/**
 * @brief Read a directory
 *
 * @param[in] dir_hdl The directory to read
 * @param[in] whence Where to start (next)
 * @param[in] dir_state Pass through of state to callback (opaque)
 * @param[in] cb Callback that receives directory entries
 * @param[out] eof [OUT] eof marker true == end of dir (no more entries)
 */
static fsal_status_t
fsal_read_dirents(struct fsal_obj_handle *dir_hdl,
                  fsal_cookie_t *whence,
                  void *dir_state,
                  fsal_readdir_cb cb,
                  bool *eof)
{
    this_fsal_obj_handle *hdl_obj = container_of(dir_hdl,
                                                 struct this_fsal_obj_handle,
                                                 obj_handle_);


    tracepoint(openvstorage_fsalovs,
               fsal_read_dirents_enter,
               (const char *)hdl_obj->id_);

    TRACE_("struct fsal_obj_handle *dir_hdl: " << dir_hdl);

    //static_assert(sizeof(fsal_cookie_t) == sizeof(vfs::Inode), "Problems");

    FileSystemWrapper::directory_entries_t dirents;
    size_t start = 0;

    if (whence != NULL)
    {
        start = *whence;
    }

    fsal_status_t status =
        fsw_exception_handler<const vfs::ObjectId&,
                              FileSystemWrapper::directory_entries_t&,
                              decltype(start)>(get_fsw(dir_hdl),
                                               &FileSystemWrapper::read_dirents,
                                               create_object_id(hdl_obj->id_),
                                               dirents,
                                               start);
    if (FSAL_IS_ERROR(status))
    {
        goto exit;
    }

    for(const auto& entry : dirents)
    {
        if(not cb(entry.c_str(),
                  dir_state,
                  start++))
        {
            OK;
        }
    }
    *eof = true;

exit:
    tracepoint(openvstorage_fsalovs,
               fsal_read_dirents_exit,
               (const char *)hdl_obj->id_,
               status.major);

    return status;
}

/**
 * @brief Create a regular file
 *
 * This function creates an empty, regular file.
 *
 * @param[in] dir_hdl Directory in which to create the file
 * @param[in] name Name of file to create
 * @param[out] attrib Attributes of newly created file
 * @param[out] handle Handle for newly created file
 *
 * @return FSAL status.
 */
static fsal_status_t
fsal_create(struct fsal_obj_handle *dir_hdl,
            const char *name,
            struct attrlist *attrib,
            struct fsal_obj_handle **handle)
{
    this_fsal_obj_handle *hdl_obj = container_of(dir_hdl,
                                                 struct this_fsal_obj_handle,
                                                 obj_handle_);

    tracepoint(openvstorage_fsalovs,
               fsal_create_enter,
               (const char *)hdl_obj->id_,
               name);

    TRACE_("struct fsal_obj_handle *dir_hdl: " << dir_hdl << std::endl
         << "const char *name: " << name << std::endl
         << "struct attrlist *attrib: " << *attrib);

    *handle = nullptr;
    this_fsal_obj_handle *hdl = nullptr;
    struct stat st;
    mode_t unix_mode;
    boost::optional<vfs::ObjectId> id;

    unix_mode = fsal2unix_mode(mode(attrib))
        & ~op_ctx->fsal_export->ops->fs_umask(op_ctx->fsal_export);

    fsal_status_t status =
        fsw_exception_handler<const vfs::ObjectId&,
                              const std::string&,
                              vfs::UserId,
                              vfs::GroupId,
                              vfs::Permissions,
                              decltype((id)),
                              struct stat&>(get_fsw(dir_hdl),
                                            &FileSystemWrapper::create,
                                            create_object_id(hdl_obj->id_),
                                            name,
                                            vfs::UserId(op_ctx->creds->caller_uid),
                                            vfs::GroupId(op_ctx->creds->caller_gid),
                                            vfs::Permissions(unix_mode),
                                            id,
                                            st);
    if (FSAL_IS_ERROR(status))
    {
        goto exit;
    }

    hdl = alloc_handle(hdl_obj->export_,
                       vfs::Inode(st.st_ino),
                       &st);
    if (not hdl)
    {
        status.major = ERR_FSAL_NOMEM;
        goto exit;
    }

    create_handle_id(hdl,
                     *id);
    *handle = &hdl->obj_handle_;
    *attrib = hdl->obj_handle_.attributes;

exit:
    tracepoint(openvstorage_fsalovs,
               fsal_create_exit,
               (const char *)hdl_obj->id_,
               name,
               (const char *)hdl->id_,
               status.major);

    return status;
}

/**
 * @brief Create a directory
 *
 * This function creates a new directory
 *
 * @param[in] dir_hdl The parent in which ro create
 * @param[in] name Name of the directory to create
 * @param[out] attrib Attributes of the newly created directory
 * @param[out] handle Handle of the newly created directory
 *
 * @return FSAL status
 */
static fsal_status_t
fsal_mkdir(struct fsal_obj_handle *dir_hdl,
           const char *name,
           struct attrlist *attrib,
           struct fsal_obj_handle **handle)
{
    this_fsal_obj_handle *hdl_obj = container_of(dir_hdl,
                                                 struct this_fsal_obj_handle,
                                                 obj_handle_);

    tracepoint(openvstorage_fsalovs,
               fsal_mkdir_enter,
               (const char *)hdl_obj->id_,
               name);

    TRACE_("struct fsal_obj_handle *dir_hdl: " << dir_hdl << std::endl
         << "const char *name: " << name
         << "struct attrlist *attrib: " << *attrib);

    *handle = nullptr;
    this_fsal_obj_handle *hdl = nullptr;
    struct stat st;
    boost::optional<vfs::ObjectId> id;

    fsal_status_t status =
        fsw_exception_handler<const vfs::ObjectId&,
                              const std::string&,
                              vfs::UserId,
                              vfs::GroupId,
                              vfs::Permissions,
                              decltype((id)),
                              struct stat&>(get_fsw(dir_hdl),
                                            &FileSystemWrapper::makedir,
                                            create_object_id(hdl_obj->id_),
                                            name,
                                            vfs::UserId(op_ctx->creds->caller_uid),
                                            vfs::GroupId(op_ctx->creds->caller_gid),
                                            vfs::Permissions(mode(attrib)),
                                            id,
                                            st);
    if (FSAL_IS_ERROR(status))
    {
        goto exit;
    }

    hdl = alloc_handle(hdl_obj->export_,
                       vfs::Inode(st.st_ino),
                       &st);
    if (not hdl)
    {
        status.major = ERR_FSAL_NOMEM;
        goto exit;
    }

    create_handle_id(hdl,
                     *id);
    *handle = &hdl->obj_handle_;

exit:
    tracepoint(openvstorage_fsalovs,
               fsal_mkdir_exit,
               (const char *)hdl_obj->id_,
               name,
               (const char *)hdl->id_,
               status.major);

    return status;
}

static fsal_status_t
fsal_mknode(struct fsal_obj_handle *dir_hdl,
            const char *name,
            object_file_type_t /*nodetype*/,
            fsal_dev_t * /*dev*/,	/* IN */
            struct attrlist *attrib,
            struct fsal_obj_handle ** /*handle*/)
{
    TRACE_("struct fsal_obj_handle *dir_hdl: " << dir_hdl << std::endl
           << "const char *name: " << name << std::endl
           << "struct attrlist *attrib: " << *attrib);

    NOK;
}

/** symlink
 *  Note that we do not set mode bits on symlinks for Linux/POSIX
 *  They are not really settable in the kernel and are not checked
 *  anyway (default is 0777) because open uses that target's mode
 */

static fsal_status_t
fsal_symlink(struct fsal_obj_handle *dir_hdl,
             const char *name,
             const char * /*link_path*/,
             struct attrlist *attrib,
             struct fsal_obj_handle ** /*handle*/)
{
    TRACE_("struct fsal_obj_handle *dir_hdl: " << dir_hdl << std::endl
         << "const char *name: " << name << std::endl
         << "struct attrlist *attrib: " << *attrib);

    NOK;
}

static fsal_status_t
fsal_readlink(struct fsal_obj_handle *obj_hdl,
              struct gsh_buffdesc * /*link_content*/,
              bool /*refresh*/)
{
    TRACE_("struct fsal_obj_handle *obj_hdl: " << obj_hdl);
    NOK;
}

static fsal_status_t
fsal_link(struct fsal_obj_handle *obj_hdl,
          struct fsal_obj_handle * /*destdir_hdl*/,
          const char * /*name*/)
{
    TRACE_("struct fsal_obj_handle *obj_hdl: " << obj_hdl);
    NOK;
}


/** @brief Rename a file
 *
 * This function renames a file, possibly moving it into another
 * directory. We assume most checks are done by the caller.
 *
 * @param[in] olddir_hdl Source directory
 * @param[in] old_name Original name
 * @param[in] newdir_hdl Destination directory
 * @param[in] new_name New name
 *
 * @return FSAL status.
 */
static fsal_status_t
fsal_rename(struct fsal_obj_handle *olddir_hdl,
            const char *old_name,
            struct fsal_obj_handle* newdir_hdl,
            const char *new_name)
{
    this_fsal_obj_handle *old_hdl_obj = container_of(olddir_hdl,
                                                     struct this_fsal_obj_handle,
                                                     obj_handle_);

    this_fsal_obj_handle *new_hdl_obj = container_of(newdir_hdl,
                                                     struct this_fsal_obj_handle,
                                                     obj_handle_);

    tracepoint(openvstorage_fsalovs,
               fsal_rename_enter,
               (const char *)old_hdl_obj->id_,
               old_name,
               (const char *)new_hdl_obj->id_,
               new_name);

    TRACE_("struct fsal_obj_handle *olddir_hdl: " << olddir_hdl << std::endl
           << "const char *old_name: " << old_name << std::endl
           << "struct fsal_obj_handle*: " << newdir_hdl << std::endl
           << "const char *new_name: " << new_name);


    fsal_status_t status =
        fsw_exception_handler<const vfs::ObjectId&,
                              const std::string&,
                              const vfs::ObjectId&,
                              const std::string&>(get_fsw(olddir_hdl),
                                                  &FileSystemWrapper::rename_file,
                                                  create_object_id(old_hdl_obj->id_),
                                                  old_name,
                                                  create_object_id(new_hdl_obj->id_),
                                                  new_name);

    tracepoint(openvstorage_fsalovs,
               fsal_rename_exit,
               (const char *)old_hdl_obj->id_,
               old_name,
               (const char *)new_hdl_obj->id_,
               new_name,
               status.major);

    return status;
}

/**
 * @brief Get attributes
 *
 * This function freshens the cached attributes stored on the handle.
 * Since the caller can take the attribute lock and read them off the
 * public filehandle, they are not copied out.
 *
 * @param[in]  obj_hdl  Object to query
 *
 * @return FSAL status.
 */
static fsal_status_t
fsal_getattrs(struct fsal_obj_handle *obj_hdl)
{
    this_fsal_obj_handle *hdl_obj = container_of(obj_hdl,
                                                 struct this_fsal_obj_handle,
                                                 obj_handle_);

    tracepoint(openvstorage_fsalovs,
               fsal_getattrs_enter,
               (const char *)hdl_obj->id_);


    TRACE_("struct fsal_obj_handle *obj_hdl: "
           << *container_of(obj_hdl, struct this_fsal_obj_handle, obj_handle_));

    struct stat st;
    fsal_status_t status = fsw_exception_handler<const vfs::ObjectId&,
                                                 struct stat&>(get_fsw(obj_hdl),
                                                               &FileSystemWrapper::getattrs,
                                                               create_object_id(hdl_obj->id_),
                                                               st);
    if (FSAL_IS_ERROR(status))
    {
        if (status.major == ERR_FSAL_NOENT)
        {
            status.major = ERR_FSAL_STALE;
        }
        goto exit;
    }
    ovs_posix_to_fsal_attributes(&st,
                                 &obj_hdl->attributes);

exit:
    tracepoint(openvstorage_fsalovs,
               fsal_getattrs_exit,
               (const char *)hdl_obj->id_,
               status.major);

    return status;
}

/**
 * @brief Set attributes on a file
 *
 * This function sets attributes on a file
 *
 * @param[in] obj_hdl File to modify
 * @param[in] attrs Attributes to set.
 *
 * @return FSAL status.
 */
static fsal_status_t
fsal_setattrs(struct fsal_obj_handle *obj_hdl,
              struct attrlist* attrs)
{

    fsal_status_t status = { ERR_FSAL_NO_ERROR, 0 };
    this_fsal_obj_handle *hdl_obj = container_of(obj_hdl,
                                                 struct this_fsal_obj_handle,
                                                 obj_handle_);

    tracepoint(openvstorage_fsalovs,
               fsal_setattrs_enter,
               (const char *)hdl_obj->id_);

    TRACE_("struct fsal_obj_handle *obj_hdl: " << obj_hdl << std::endl
         << "struct attrlist *attrib: " << *attrs);

    if (FSAL_TEST_MASK(attrs->mask, ATTR_SIZE))
    {
        if (obj_hdl->type != REGULAR_FILE)
        {
            status.major = ERR_FSAL_INVAL;
            goto exit;
        }

        status = fsw_exception_handler<const vfs::ObjectId&,
                                       off_t>(get_fsw(obj_hdl),
                                              &FileSystemWrapper::truncate,
                                              create_object_id(hdl_obj->id_),
                                              attrs->filesize);
    }
    else if(FSAL_TEST_MASK(attrs->mask, ATTR_MODE))
    {
        status = fsw_exception_handler<const vfs::ObjectId&,
                                       decltype(attrs->mode)>(get_fsw(obj_hdl),
                                                              &FileSystemWrapper::chmod,
                                                              create_object_id(hdl_obj->id_),
                                                              fsal2unix_mode(attrs->mode));
    }
    else if(FSAL_TEST_MASK(attrs->mask, ATTR_OWNER bitor ATTR_GROUP))
    {
        uid_t uid = FSAL_TEST_MASK(attrs->mask, ATTR_OWNER) ?
            static_cast<uid_t>(attrs->owner) :
            -1;

        gid_t gid = FSAL_TEST_MASK(attrs->mask, ATTR_GROUP) ?
            static_cast<gid_t>(attrs->owner) :
            -1;

        status = fsw_exception_handler<const vfs::ObjectId&,
                                       decltype(uid),
                                       decltype(gid)>(get_fsw(obj_hdl),
                                                      &FileSystemWrapper::chown,
                                                      create_object_id(hdl_obj->id_),
                                                      uid,
                                                      gid);
    }
    else if(FSAL_TEST_MASK(attrs->mask, ATTR_ATIME | ATTR_MTIME |
                           ATTR_ATIME_SERVER | ATTR_MTIME_SERVER))
    {
        struct stat stat;
        struct timespec timebuf[2];

        status = fsw_exception_handler<const vfs::ObjectId&,
                                       struct stat&>(get_fsw(obj_hdl),
                                                     &FileSystemWrapper::getattrs,
                                                     create_object_id(hdl_obj->id_),
                                                     stat);
        if (FSAL_IS_ERROR(status))
        {
            goto exit;
        }

        if (FSAL_TEST_MASK(attrs->mask, ATTR_ATIME_SERVER))
        {
            int rc = clock_gettime(CLOCK_REALTIME, &timebuf[0]);
            if (rc != 0)
            {
                std::error_code ec(errno,
                                   std::generic_category());
                status.major = std_system_to_fsal_error(ec);
                goto exit;
            }
        }
        else if (FSAL_TEST_MASK(attrs->mask, ATTR_ATIME))
        {
            timebuf[0] = attrs->atime;
        }
        else
        {
            timebuf[0].tv_sec = stat.st_atime;
            timebuf[0].tv_nsec = 0;
        }

        if (FSAL_TEST_MASK(attrs->mask, ATTR_MTIME_SERVER))
        {
            int rc = clock_gettime(CLOCK_REALTIME,
                                   &timebuf[1]);
            if (rc != 0)
            {
                std::error_code ec(errno,
                                   std::generic_category());
                status.major = std_system_to_fsal_error(ec);
                goto exit;
            }
        }
        else if (FSAL_TEST_MASK(attrs->mask, ATTR_MTIME))
        {
            timebuf[1] = attrs->mtime;
        }
        else
        {
            timebuf[1].tv_sec = stat.st_mtime;
            timebuf[1].tv_nsec = 0;
        }

        status = fsw_exception_handler<const vfs::ObjectId&,
                                       const struct timespec*>(get_fsw(obj_hdl),
                                                               &FileSystemWrapper::utimes,
                                                               create_object_id(hdl_obj->id_),
                                                               timebuf);
    }
    else
    {
        VERIFY(0 == "Unsupported settattrs call");
    }

exit:
    tracepoint(openvstorage_fsalovs,
               fsal_setattrs_exit,
               (const char *)hdl_obj->id_,
               status.major);

    return status;
}

/**
 * @brief Remove a name
 *
 * This function removes a name from the filesystem and possibly
 * deletes the associated file. Directories must be empty to be
 * removed.
 *
 * @param[in] dir_hdl Parent directory
 * @Param[in] name Name to remove
 *
 * @return FSAL status.
 */
static fsal_status_t
fsal_unlink(struct fsal_obj_handle* dir_hdl,
            const char* name)
{
    this_fsal_obj_handle *hdl_obj = container_of(dir_hdl,
                                                 struct this_fsal_obj_handle,
                                                 obj_handle_);

    tracepoint(openvstorage_fsalovs,
               fsal_unlink_enter,
               (const char *)hdl_obj->id_,
               name);

    TRACE_("struct fsal_obj_handle *dir_hdl: " << dir_hdl << std::endl
           << "const char* name: " << name);


    fsal_status_t status =
        fsw_exception_handler<const vfs::ObjectId&,
                              const char*>(get_fsw(dir_hdl),
                                           &FileSystemWrapper::file_unlink,
                                           create_object_id(hdl_obj->id_),
                                           name);

    tracepoint(openvstorage_fsalovs,
               fsal_unlink_exit,
               (const char *)hdl_obj->id_,
               name,
               status.major);

    return status;
}

/**
 * @brief Write wire handle
 *
 * This function writes a "wire" handle to be sent to clients and
 * received from them.
 *
 * @param[in]     obj_hdl     The handle to digest
 * @param[in]     output_type The type of digest requested
 * @param[in,out] fh_desc     Location/size of buffer for digest/length
 *                            modified to digest length.
 *
 * @return FSAL status
 */
static fsal_status_t
fsal_handle_digest(const struct fsal_obj_handle *obj_hdl,
                   fsal_digesttype_t output_type,
                   struct gsh_buffdesc *fh_desc)
{

    fsal_status_t status = { ERR_FSAL_NO_ERROR, 0 };
    this_fsal_obj_handle *hdl_obj = container_of(obj_hdl,
                                                 struct this_fsal_obj_handle,
                                                 obj_handle_);

    tracepoint(openvstorage_fsalovs,
               fsal_handle_digest_enter,
               (const char *)hdl_obj->id_);

    TRACE_("const struct fsal_obj_handle *obj_hdl: " << obj_hdl);

    switch (output_type)
    {
        /* Digested Handles */
    case FSAL_DIGEST_NFSV3:
    case FSAL_DIGEST_NFSV4:
        if (fh_desc->len < UUID_LENGTH)
        {
            status.major = ERR_FSAL_TOOSMALL;
            break;
        }
        memcpy(fh_desc->addr,
               hdl_obj->id_,
               UUID_LENGTH);
        fh_desc->len = UUID_LENGTH;
        break;
    default:
        status.major = ERR_FSAL_SERVERFAULT;
    }

    tracepoint(openvstorage_fsalovs,
               fsal_handle_digest_exit,
               (const char *)hdl_obj->id_,
               status.major);

    return status;
}

/**
 * @brief Give a hash key for file handle
 *
 * Thsi function locates a unique hash key for a given file.
 *
 * @param[in]  obj_hdl Handle whose key is to be got
 * @param[out] fh_desc Address and length of key
 */
static void
fsal_handle_to_key(struct fsal_obj_handle *obj_hdl,
                   struct gsh_buffdesc *fh_desc)
{
    struct this_fsal_obj_handle *objhandle;
    objhandle = container_of(obj_hdl,
                             struct this_fsal_obj_handle,
                             obj_handle_);

    tracepoint(openvstorage_fsalovs,
               fsal_handle_to_key_enter,
               (const char *)objhandle->id_);

    auto on_exit(yt::make_scope_exit([&]{
                tracepoint(openvstorage_fsalovs,
                           fsal_handle_to_key_exit,
                           (const char *)objhandle->id_,
                           std::uncaught_exception());
                }));

    TRACE_("struct fsal_obj_handle *obj_hdl: " << obj_hdl <<
           "inode: " << objhandle->inode_);

    fh_desc->addr = &objhandle->id_;
    fh_desc->len = UUID_LENGTH;
}

#ifdef SUPPORT_XATTR
static fsal_status_t
list_ext_attrs(struct fsal_obj_handle * /*obj_hdl*/,
               unsigned int /*argcookie*/,
               fsal_xattrent_t * /*xattrs_tab*/,
               unsigned int /*xattrs_tabsize*/,
               unsigned int * /*p_nb_returned*/,
               int * /*end_of_list*/)
{
    OK;
}

static fsal_status_t
getextattr_id_by_name(struct fsal_obj_handle * /*obj_hdl*/,
                      const char * /*xattr_name*/,
                      unsigned int * /*pxattr_id*/)
{
    OK;
}


static fsal_status_t
getextattr_value_by_id(struct fsal_obj_handle * /*obj_hdl*/,
                       unsigned int /*xattr_id*/,
                       caddr_t /*buffer_addr*/,
                       size_t /*buffer_size*/,
                       size_t * /*p_output_size*/)
{
    OK;
}

static fsal_status_t
getextattr_value_by_name(struct fsal_obj_handle * /*obj_hdl*/,
                         const char * /*xattr_name*/,
                         caddr_t /*buffer_addr*/,
                         size_t /*buffer_size*/,
                         size_t * /*p_output_size*/)
{
    OK;
}

static fsal_status_t
setextattr_value(struct fsal_obj_handle * /*obj_hdl*/,
                 const char * /*xattr_name*/,
                 caddr_t /*buffer_addr*/,
                 size_t /*buffer_size*/,
                 int /*create*/)
{
    OK;
}

static fsal_status_t
setextattr_value_by_id(struct fsal_obj_handle * /*obj_hdl*/,
                       unsigned int /*xattr_id*/,
                       caddr_t /*buffer_addr*/,
                       size_t /*buffer_size*/)
{
    OK;
}

static fsal_status_t
getextattr_attrs(struct fsal_obj_handle * /*obj_hdl*/,
                 unsigned int /*xattr_id*/,
                 struct attrlist * /*p_attrs*/)
{
    OK;
}

static fsal_status_t
remove_extattr_by_id(struct fsal_obj_handle * /*obj_hdl*/,
                     unsigned int /*xattr_id*/)
{
    OK;
}

static fsal_status_t
remove_extattr_by_name(struct fsal_obj_handle * /*obj_hdl*/,
                       const char * /*xattr_name*/)
{
    OK;
}
#endif // SUPPORT_XATTR

/**
 * @brief Open a file for read or write
 *
 * This function opens a file for reading or writing. No lock is taken
 * because we assume we are protected by the cache inode content lock.
 *
 * @param[in] obj_hdl File to open
 * @param[in] openflags Mode to open in.
 *
 * @return FSAL status.
 */
static fsal_status_t
fsal_open(struct fsal_obj_handle *obj_hdl,
          fsal_openflags_t /* openflags */)
{

    mode_t posix_flags = O_RDWR;
    fsal_status_t status = { ERR_FSAL_NO_ERROR, 0 };

    this_fsal_obj_handle *handle_obj = container_of(obj_hdl,
                                                    struct this_fsal_obj_handle,
                                                    obj_handle_);

    tracepoint(openvstorage_fsalovs,
               fsal_open_enter,
               (const char *)handle_obj->id_);

    TRACE_("struct fsal_obj_handle *obj_hdl: " << obj_hdl);
    // fsal2posix_openflags(openflags, &posix_flags);
    if (handle_obj->openflags != FSAL_O_CLOSED)
    {
        status.major = ERR_FSAL_SERVERFAULT;
        goto exit;
    }

    status = fsw_exception_handler<const vfs::ObjectId&,
                                   mode_t,
                                   ganesha::FSData&>(get_fsw(obj_hdl),
                                                     &FileSystemWrapper::open,
                                                     create_object_id(handle_obj->id_),
                                                     posix_flags,
                                                     handle_obj->fsdata);
    if (FSAL_IS_ERROR(status))
    {
        goto exit;
    }
    handle_obj->openflags = FSAL_O_RDWR;

exit:
    tracepoint(openvstorage_fsalovs,
               fsal_open_exit,
               (const char *)handle_obj->id_,
               status.major);

    return status;
}

/**
 * @brief Return the open status of a file
 *
 * This function returns the open status for a given file.
 *
 * @param[in] obj_hdl File to get status.
 *
 * @return Open mode.
 */
static fsal_openflags_t
fsal_status(struct fsal_obj_handle *obj_hdl)
{
    this_fsal_obj_handle *handle_obj = container_of(obj_hdl,
                                                    struct this_fsal_obj_handle,
                                                    obj_handle_);

    tracepoint(openvstorage_fsalovs,
               fsal_status_enter,
               (const char *)handle_obj->id_);

    auto on_exit(yt::make_scope_exit([&]{
                tracepoint(openvstorage_fsalovs,
                           fsal_status_exit,
                           (const char *)handle_obj->id_,
                           std::uncaught_exception());
                }));

    TRACE_("struct fsal_obj_handle *obj_hdl: " << obj_hdl);

    return handle_obj->openflags;
}

/**
 * Read data from a file
 *
 * This function reads data from an open file.
 *
 * We take no lock, since we assume we are protected by the cache inode
 * content lock.
 *
 * @param[in] obj_hdl File to read
 * @param[in] offset Point at which to begin read
 * @param[in] buffer_size Maximum number of bytes to read
 * @param[out] buffer Buffer to store data read
 * @param[out] read_amount Count of bytes to read
 * @param[out] end_of_file true if the end of file is reached
 *
 * @return FSAL status.
 */
static fsal_status_t
fsal_read(struct fsal_obj_handle *obj_hdl,
          uint64_t offset,
          size_t buffer_size,
          void *buffer,
          size_t *read_amount,
          bool *end_of_file)
{
    fsal_status_t status = { ERR_FSAL_NO_ERROR, 0 };
    this_fsal_obj_handle *handle_obj = container_of(obj_hdl,
                                                    struct this_fsal_obj_handle,
                                                    obj_handle_);

    tracepoint(openvstorage_fsalovs,
               fsal_read_enter,
               (const char *)handle_obj->id_,
               offset,
               buffer_size);

    TRACE_("struct fsal_obj_handle *obj_hdl: " << obj_hdl << std::endl
           << "uint64_t offset: " << offset << std::endl
           << "size_t buffer_size: " << buffer_size << std::endl
           << "void *buffer: " << buffer);

#if 0
    /* NULL I/O */
    *read_amount = buffer_size;
    *end_of_file = false;
    OK;
#endif

    status = fsw_exception_handler<const ganesha::FSData&,
                                   decltype(offset),
                                   decltype(buffer_size),
                                   void*,
                                   size_t&,
                                   bool&>(get_fsw(obj_hdl),
                                          &FileSystemWrapper::read,
                                          handle_obj->fsdata,
                                          offset,
                                          buffer_size,
                                          buffer,
                                          *read_amount,
                                          *end_of_file);

    tracepoint(openvstorage_fsalovs,
               fsal_read_exit,
               (const char *)handle_obj->id_,
               offset,
               buffer_size,
               status.major);

    return status;
}

/**
 * @brief Write data to a file
 *
 * This function writes data to an open file.
 *
 * We take no lock, since we assume we are protected by the cache inode
 * content lock.
 *
 * @note Should buffer be const? -- ACE
 *
 * @param[in]  obj_hdl      File to be written
 * @param[in]  offset       Position at which to write
 * @param[in]  buffer_size  Number of bytes to write
 * @param[in]  buffer       Data to be written
 * @param[out] write_amount Number of bytes written
 * @param[in,out] fsal_stable In, if on, the fsal is requested to write data
 *                            to stable store. Out, the fsal reports what
 *                            it did.
 *
 * @return FSAL status.
 */
static fsal_status_t
fsal_write(struct fsal_obj_handle *obj_hdl,
           uint64_t offset,
           size_t buffer_size,
           void *buffer,
           size_t *write_amount,
           bool *fsal_stable)
{
    fsal_status_t status = { ERR_FSAL_NO_ERROR, 0 };
    this_fsal_obj_handle *handle_obj = container_of(obj_hdl,
                                                    struct this_fsal_obj_handle,
                                                    obj_handle_);

    tracepoint(openvstorage_fsalovs,
               fsal_write_enter,
               (const char *)handle_obj->id_,
               offset,
               buffer_size);

    TRACE_("struct fsal_obj_handle *obj_hdl: " << obj_hdl << std::endl
          << "uint64_t offset: " << offset << std::endl
          << "size_t buffer_size: " << buffer_size << std::endl
           << "void *buffer: " << buffer);

#if 0
    /* NULL I/O */
    *write_amount = buffer_size;
    *fsal_stable = false;
    OK;
#endif

    status = fsw_exception_handler<const ganesha::FSData&,
                                   decltype(offset),
                                   decltype(buffer_size),
                                   const void*,
                                   size_t&,
                                   bool&>(get_fsw(obj_hdl),
                                          &FileSystemWrapper::write,
                                          handle_obj->fsdata,
                                          offset,
                                          buffer_size,
                                          buffer,
                                          *write_amount,
                                          *fsal_stable);

    tracepoint(openvstorage_fsalovs,
               fsal_write_exit,
               (const char *)handle_obj->id_,
               offset,
               buffer_size,
               status.major);

    return status;
}

/**
 * @brief Commit written data
 *
 * This function commits written data to stable storage
 *
 * @param[in] obj_hdl File to commit
 * @param[in] offset  Start of range to commit
 * @param[in] len     Size of range to commit
 *
 * @return FSAL status.
 */
static fsal_status_t
fsal_commit(struct fsal_obj_handle *obj_hdl,	/* sync */
            off_t offset,
            size_t len)
{
    fsal_status_t status = { ERR_FSAL_NO_ERROR, 0 };
    this_fsal_obj_handle *handle_obj = container_of(obj_hdl,
                                                    struct this_fsal_obj_handle,
                                                    obj_handle_);

    tracepoint(openvstorage_fsalovs,
               fsal_commit_enter,
               (const char *)handle_obj->id_,
               offset,
               len);

    TRACE_("struct fsal_obj_handle *obj_hdl: " << obj_hdl << std::endl
           << "off_t offset: " << offset << std::endl
           << "size_t len: " << len);

#if 0
    /* NULL I/O */
    OK;
#endif

    status = fsw_exception_handler<const ganesha::FSData&,
                                    decltype(offset),
                                    decltype(len)>(get_fsw(obj_hdl),
                                                   &FileSystemWrapper::commit,
                                                   handle_obj->fsdata,
                                                   offset,
                                                   len);

    tracepoint(openvstorage_fsalovs,
               fsal_commit_exit,
               (const char *)handle_obj->id_,
               offset,
               len,
               status.major);

    return status;
}

/**
 * @brief Perform a lock operation
 *
 * This function performs a lock operation (lock, unlock, test) on a
 * file.
 *
 * @param[in]  obj_hdl          File on which to operate
 * @param[in]  p_owner          Lock owner (Not yet implemented)
 * @param[in]  lock_op          Operation to perform
 * @param[in]  request_lock     Lock to take/release/test
 * @param[out] conflicting_lock Conflicting lock
 *
 * @return FSAL status.
 */
static fsal_status_t
fsal_lock_op(struct fsal_obj_handle *obj_hdl,
             void *p_owner,
             fsal_lock_op_t lock_op,
             fsal_lock_param_t *request_lock,
             fsal_lock_param_t *conflicting_lock)
{
    TRACE_("struct fsal_obj_handle *obj_hdl: " << obj_hdl << std::endl
           << "fsal_lock_op_t lock_op: " << lockOp(lock_op) << std::endl
           << "fsal_lock_param_t *request_lock: " << *request_lock << std::endl
           << "fsal_lock_param_t *conflicting_lock: " << (conflicting_lock ? "was set" : "was not set"));

    (void)p_owner;

    //if (p_owner != NULL) {
        //return fsalstat(ERR_FSAL_NOTSUPP,0);
    //}
    OK;
}

/**
 * @brief Close a file
 *
 * This function closes a file,
 * Yes, we ignore lock status.  Closing a file in POSIX
 * releases all locks but that is state and cache inode's problem.
 *
 * @param[in] obj_hdl File to close
 *
 * @return FSAL status.
 */
static fsal_status_t
fsal_close(struct fsal_obj_handle *obj_hdl)
{

    this_fsal_obj_handle *handle_obj = container_of(obj_hdl,
                                                    struct this_fsal_obj_handle,
                                                    obj_handle_);

    tracepoint(openvstorage_fsalovs,
               fsal_close_enter,
               (const char *)handle_obj->id_);

    TRACE_("struct fsal_obj_handle *obj_hdl: " << obj_hdl << std::endl);

    fsal_status_t status =
        fsw_exception_handler<ganesha::FSData&>(get_fsw(obj_hdl),
                                                &FileSystemWrapper::close,
                                                handle_obj->fsdata);
    if (FSAL_IS_ERROR(status))
    {
        goto exit;
    }
    handle_obj->openflags = FSAL_O_CLOSED;
exit:

    tracepoint(openvstorage_fsalovs,
               fsal_close_exit,
               (const char *)handle_obj->id_,
               status.major);

    return status;
}

/* lru_cleanup
 * free non-essential resources at the request of cache inode's
 * LRU processing identifying this handle as stale enough for resource
 * trimming.
 */
static fsal_status_t
fsal_lru_cleanup(struct fsal_obj_handle *obj_hdl,
                 lru_actions_t requests)
{
    fsal_status_t status = { ERR_FSAL_NO_ERROR, 0 };
    this_fsal_obj_handle *handle_obj = container_of(obj_hdl,
                                                    struct this_fsal_obj_handle,
                                                    obj_handle_);

    tracepoint(openvstorage_fsalovs,
               fsal_lru_cleanup_enter,
               (const char *)handle_obj->id_);

    TRACE_("struct fsal_obj_handle *obj_hdl: " << obj_hdl);

    status = fsw_exception_handler<const vfs::ObjectId&,
                                   decltype(LRUCloseFiles::T),
                                   decltype(LRUFreeMemory::F)>(get_fsw(obj_hdl),
                                           &FileSystemWrapper::lru_cleanup,
                                           create_object_id(handle_obj->id_),
                                           requests bitand LRU_CLOSE_FILES ? LRUCloseFiles::T : LRUCloseFiles::F,
                                           requests bitand LRU_FREE_MEMORY ? LRUFreeMemory::T : LRUFreeMemory::F);

    tracepoint(openvstorage_fsalovs,
               fsal_lru_cleanup_exit,
               (const char *)handle_obj->id_,
               status.major);

    return status;
}

} //namespace

/**
 * @brief Set handle ops vector.
 *
 * @param[in] ops Handle operations vector
 */
void handle_ops_init(struct fsal_obj_ops *ops)
{
    ops->release = fsal_release_handle;
    ops->lookup = fsal_lookup;
    ops->readdir = fsal_read_dirents;
    ops->create = fsal_create;
    ops->mkdir = fsal_mkdir;
    ops->mknode = fsal_mknode;
    ops->symlink = fsal_symlink;
    ops->readlink = fsal_readlink;
    ops->test_access = fsal_test_access;
    ops->getattrs = fsal_getattrs;
    ops->setattrs = fsal_setattrs;
    ops->link = fsal_link;
    ops->rename = fsal_rename;
    ops->unlink = fsal_unlink;
    ops->open = fsal_open;
    ops->status = fsal_status;
    ops->read = fsal_read;
    ops->write = fsal_write;
    ops->commit = fsal_commit;
    ops->lock_op = fsal_lock_op;
    ops->close = fsal_close;
    ops->lru_cleanup = fsal_lru_cleanup;
    ops->handle_digest = fsal_handle_digest;
    ops->handle_to_key = fsal_handle_to_key;

    /* LETS HOPE WE DONT NEED THESE */
#if SUPPORT_XATTR
    ops->list_ext_attrs = list_ext_attrs;
    ops->getextattr_id_by_name = getextattr_id_by_name;
    ops->getextattr_value_by_name = getextattr_value_by_name;
    ops->getextattr_value_by_id = getextattr_value_by_id;
    ops->setextattr_value = setextattr_value;
    ops->setextattr_value_by_id = setextattr_value_by_id;
    ops->getextattr_attrs = getextattr_attrs;
    ops->remove_extattr_by_id = remove_extattr_by_id;
    ops->remove_extattr_by_name = remove_extattr_by_name;
#endif // SUPPORT_XATTR
}
