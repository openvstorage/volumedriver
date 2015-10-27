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

extern "C" {
#include <config_parsing.h>
#include <FSAL/fsal_config.h>
#include <FSAL/fsal_init.h>
#include <config_parsing.h>
#include <nfs_exports.h>
#include <export_mgr.h>
}

namespace
{

using namespace ganesha;
namespace vfs = volumedriverfs;
namespace yt = youtils;

struct this_fsal_export*
this_export(struct fsal_export* exp_hdl)
{
    return container_of(exp_hdl,
                        struct this_fsal_export,
                        export_);
}

struct this_fsal_module*
module(struct fsal_module* module)
{
    return container_of(module,
                        struct this_fsal_module,
                        fsal);
}

struct fsal_staticfsinfo_t*
staticinfo(struct fsal_module* hdl)
{
    return &module(hdl)->fs_info;
}

/**
 * @brief Implements OVS FSAL export operation release
 */
static void release_export(struct fsal_export *exp_hdl)
{
    TRACE_("struct fsal_export *exp_hdl: " << exp_hdl);

    auto myself = this_export(exp_hdl);

    /* detach export */
    fsal_detach_export(myself->export_.fsal,
                       &myself->export_.exports);
    free_export_ops(&myself->export_);
    myself->export_.ops = NULL;

    /* Stop OVS discovery tcp server */
    INFO("Stopping discovery tcp server");
    myself->server_->stop();
    delete myself->server_;
    /* OVS cleanup */
    delete myself->wrapper_;
    /*
     * Teardown logging while releasing the export and not on module
     * unload()'ing because unload() is defined as a destructor (MODULE_FINI)
     * called after the main() has finished execution or exit() has been
     * called.
     * So, static objects are already destroyed/deinitialized and every try
     * using them causes undefined behavior.
     */
    yt::Logger::teardownLogging();

    /* memory cleanup */
    gsh_free(myself);
    myself = nullptr;
}

static fsal_status_t get_fs_dynamic_info(struct fsal_export *exp_hdl,
                                         struct fsal_obj_handle * /*obj_hdl*/,
                                         fsal_dynamicfsinfo_t *infop)
{
    struct statvfs st;
    fsal_status_t status =
        fsw_exception_handler<struct statvfs&>(get_fsw(exp_hdl),
                                               &FileSystemWrapper::statvfs,
                                               st);
    if (FSAL_IS_ERROR(status))
    {
        return status;
    }

    memset(infop, 0, sizeof(fsal_dynamicfsinfo_t));
    infop->total_bytes = st.f_frsize * st.f_blocks;
    infop->free_bytes = st.f_frsize * st.f_bfree;
    infop->avail_bytes = st.f_frsize * st.f_bavail;
    infop->total_files = st.f_files;
    infop->free_files = st.f_ffree;
    infop->avail_files = st.f_favail;
    infop->time_delta.tv_sec = 1;
    infop->time_delta.tv_nsec = 0;

    return status;
}

/**
 * @brief Query FSAL's capabilities
 *
 * This function queries the capabilities of an FSAL export.
 *
 * @param[in] exp_hdl The public export handle
 * @param[in] option  The option to check
 *
 * @return True/False if option is supported or not.
 */
static bool fs_supports(struct fsal_export *exp_hdl,
        fsal_fsinfo_options_t option)
{
    return fsal_supports(staticinfo(exp_hdl->fsal),
                         option);
}

/**
 * @brief Return the longest file supported
 *
 * This function returns the length of the longest file supported.
 *
 * @param[in] exp_hdl The public export
 *
 * @return UINT64_MAX.
 */
static uint64_t
fs_maxfilesize(struct fsal_export *exp_hdl)
{
    return fsal_maxfilesize(staticinfo(exp_hdl->fsal));
}

/**
 * @brief Return the longest read supported
 *
 * This function returns the length of the longest read supported.
 *
 * @param[in] export_pub The public export
 *
 * @return UINT32_MAX.
 */
static uint32_t
fs_maxread(struct fsal_export *exp_hdl)
{
    return fsal_maxread(staticinfo(exp_hdl->fsal));
}

/**
 * @brief Return the longest write supported
 *
 * This function returns the length of the longest write supported.
 *
 * @param[in] exp_hdl The public export
 *
 * @return UINT32_MAX.
 */
static uint32_t
fs_maxwrite(struct fsal_export *exp_hdl)
{
    return fsal_maxwrite(staticinfo(exp_hdl->fsal));
}

/**
 * @brief Return the maximum number of hard links to a file
 *
 * This function returns the maximum number of hard links supported to
 * any file.
 *
 * @param[in] exp_hdl The public export
 *
 * @return UINT32_MAX.
 */
static uint32_t
fs_maxlink(struct fsal_export *exp_hdl)
{
    return fsal_maxlink(staticinfo(exp_hdl->fsal));
}

/**
 * @brief Return the maximum size of a filename
 *
 * This function returns the maximum filename length.
 *
 * @param[in] exp_hdl The public export
 *
 * @return UINT32_MAX.
 */
static uint32_t
fs_maxnamelen(struct fsal_export *exp_hdl)
{
    return fsal_maxnamelen(staticinfo(exp_hdl->fsal));
}

/**
 * @brief Return the maximum length of a path
 *
 * This function returns the maximum path length.
 *
 * @param[in] exp_hdl The public export
 *
 * @return UINT32_MAX.
 */
static uint32_t
fs_maxpathlen(struct fsal_export *exp_hdl)
{
    return fsal_maxpathlen(staticinfo(exp_hdl->fsal));
}

/**
 * @brief Return the lease time
 *
 * This function returns the lease time.
 *
 * @param[in] exp_hdl The public export
 *
 */
static struct timespec
fs_lease_time(struct fsal_export *exp_hdl)
{
    return fsal_lease_time(staticinfo(exp_hdl->fsal));
}

/**
 * @brief Return ACL support
 *
 * This function returns the export's ACL support.
 *
 * @param[in] exp_hdl The public export
 *
 */
static fsal_aclsupp_t
fs_acl_support(struct fsal_export *exp_hdl)
{
    return fsal_acl_support(staticinfo(exp_hdl->fsal));
}

/**
 * @brief Return the attributes supported by this FSAL
 *
 * This function returns the mask of attributes this FSAL can support.
 *
 * @param[in] exp_hdl The public export
 *
 */
static attrmask_t
fs_supported_attrs(struct fsal_export *exp_hdl)
{
    return fsal_supported_attrs(staticinfo(exp_hdl->fsal));
}

/**
 * @brief Return the mode under which the FSAL will create files
 *
 * This function returns the default mode on any new file created.
 *
 * @param[in] exp_hdl The public export
 *
 */
static uint32_t
fs_umask(struct fsal_export *exp_hdl)
{
    return fsal_umask(staticinfo(exp_hdl->fsal));
}

/**
 * @brief Return the mode for extended attributes
 *
 * This function returns the access mode applied to extended
 * attributes.  This seems a bit dubious
 *
 * @param[in] export_pub The public export
 *
 */
static uint32_t
fs_xattr_access_rights(struct fsal_export *exp_hdl)
{
    return fsal_xattr_access_rights(staticinfo(exp_hdl->fsal));
}

#ifdef QUOTA_SUPPORT
/* get_quota
 * return quotas for this export.
 * path could cross a lower mount boundary which could
 * mask lower mount values with those of the export root
 * if this is a real issue, we can scan each time with setmntent()
 * better yet, compare st_dev of the file with st_dev of root_fd.
 * on linux, can map st_dev -> /proc/partitions name -> /dev/<name>
 */
static fsal_status_t get_quota(struct fsal_export * /*exp_hdl*/,
                               const char * /*filepath*/,
                               int /*quota_type*/,
                               struct req_op_context * /*req_ctx*/,
                               fsal_quota_t * /*pquota*/)
{
    OK;
}

/* set_quota
 * same lower mount restriction applies
 */
static fsal_status_t set_quota(struct fsal_export * /*exp_hdl*/,
                               const char * /*filepath*/,
                               int /*quota_type*/,
                               struct req_op_context * /*req_ctx*/,
                               fsal_quota_t * /*pquota*/,
                               fsal_quota_t * /*presquota*/)
{
    OK;
}
#endif

/* extract a file handle from a buffer.
 * do verification checks and flag any and all suspicious bits.
 * Return an updated fh_desc into whatever was passed.  The most
 * common behavior, done here is to just reset the length.  There
 * is the option to also adjust the start pointer.
 */
static fsal_status_t
extract_handle(struct fsal_export * /*exp_hdl*/,
               fsal_digesttype_t in_type,
               struct gsh_buffdesc *fh_desc)
{
    TRACE_("fsal_digesttype_t in_type: " << in_type << std::endl
           << "struct gsh_buffdesc *fh_desc: " << fh_desc);

    /* sanity checks */
    if (!fh_desc || !fh_desc->addr)
    {
        return fsalstat(ERR_FSAL_FAULT, 0);
    }

    //FIXME What about FSAL_DIGEST_NFSV2
    return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

/**
 * @brief Return a handle corresponding to a path.
 *
 * This function looks up the given path and supplies an FSAL
 * object.
 *
 * @param[in] exp_hdl    The export in which to look up the file.
 * @param[in] path       The path to look up.
 * @param[out] handle    The created public FSAL handle.
 *
 * @return FSAL status.
 */
static fsal_status_t
lookup_path(struct fsal_export *exp_hdl,
            const char *path,
            struct fsal_obj_handle **handle)
{
    tracepoint(openvstorage_fsalovs,
               lookup_path_enter,
               path);

    TRACE_("struct fsal_export *exp_hdl: " << exp_hdl << std::endl
           << "const char* path: " << path);

    fsal_status_t status = { ERR_FSAL_NO_ERROR, 0 };
    *handle = nullptr;
    boost::optional<vfs::ObjectId> id;
    struct stat st;
    auto *hdl =
        (this_fsal_obj_handle*) gsh_calloc(1,
                sizeof(struct this_fsal_obj_handle));

    if (not hdl)
    {
        status.major = ERR_FSAL_NOMEM;
        goto exit;
    }

    status = fsw_exception_handler<const std::string&,
                                   ganesha::FSData&,
                                   decltype((id)),
                                   struct stat&>(get_fsw(exp_hdl),
                                                 &FileSystemWrapper::lookup_path,
                                                 path,
                                                 hdl->fsdata,
                                                 id,
                                                 st);
    if (FSAL_IS_ERROR(status))
    {
        goto exit;
    }
    create_handle_id(hdl,
                     *id);

    hdl->inode_ = vfs::Inode(st.st_ino);
    hdl->export_ = exp_hdl;
    hdl->obj_handle_.attributes.mask = exp_hdl->ops->fs_supported_attrs(exp_hdl);
    ovs_posix_to_fsal_attributes(&st,
                                 &hdl->obj_handle_.attributes);
    fsal_obj_handle_init(&hdl->obj_handle_,
                         exp_hdl,
                         hdl->obj_handle_.attributes.type);
    *handle = &hdl->obj_handle_;

exit:
    tracepoint(openvstorage_fsalovs,
               lookup_path_exit,
               path,
               (const char *)hdl->id_,
               status.major);

    return status;
}

/**
 * @brief Create a FSAL object handle from a wire handle
 *
 * This function creates a FSAL object handle from a client supplied
 * "wire" handle (when an object is no longer in cache but the client
 * still remembers the handle).
 *
 * @param[in]  exp_hdl  The export in which to create the handle
 * @param[in]  hdl_desc Buffer descriptor for the "wire" handle
 * @param[out] handle   FSAL object handle
 *
 * @return FSAL status.
 */
static fsal_status_t
create_handle(struct fsal_export *exp_hdl,
              struct gsh_buffdesc *hdl_desc,
              struct fsal_obj_handle **handle)
{

    tracepoint(openvstorage_fsalovs,
               create_handle_enter,
               (char *)hdl_desc->addr,
               hdl_desc->len);

    TRACE_("struct fsal_export *exp_hdl: " << exp_hdl << std::endl
           << "struct gsh_buffdesc *hdl_desc: " << hdl_desc << std::endl
           << "hdl_desc->addr: " << hdl_desc->addr << std::endl
           << "hdl_desc->len: " <<  hdl_desc->len);

    fsal_status_t status = { ERR_FSAL_NO_ERROR, 0 };
    *handle = nullptr; /* poison it first */
    this_fsal_obj_handle *hdl = nullptr;

    if(hdl_desc->len != UUID_LENGTH)
    {
        status.major = ERR_FSAL_SERVERFAULT;
        goto exit;
    }

    struct stat st;
    status =
        fsw_exception_handler<const vfs::ObjectId&,
                              struct stat&>(get_fsw(exp_hdl),
                                            &FileSystemWrapper::getattrs,
                                            create_object_id((unsigned char*)hdl_desc->addr),
                                            st);
    if (FSAL_IS_ERROR(status))
    {
        goto exit;
    }

    hdl = alloc_handle(exp_hdl,
                            vfs::Inode(st.st_ino),
                            &st);
    if (not hdl)
    {
        status.major = ERR_FSAL_NOMEM;
        goto exit;
    }

    memcpy(hdl->id_,
           hdl_desc->addr,
           UUID_LENGTH);
    hdl->id_[UUID_LENGTH] = '\0';
    *handle = &hdl->obj_handle_;

exit:
    tracepoint(openvstorage_fsalovs,
               create_handle_exit,
               (char *)hdl_desc->addr,
               hdl_desc->len,
               (const char *)hdl->id_,
               status.major);

    return status;
}

} //namespace

/**
 * @brief Set operations for exports
 *
 * @param[in, out] ops Operations vector.
 */
void export_ops_init(struct export_ops *ops)
{
    ops->release = release_export;
    ops->lookup_path = lookup_path;
    ops->extract_handle = extract_handle;
    ops->create_handle = create_handle;
    ops->get_fs_dynamic_info = get_fs_dynamic_info;
    ops->fs_supports = fs_supports;
    ops->fs_maxfilesize = fs_maxfilesize;
    ops->fs_maxread = fs_maxread;
    ops->fs_maxwrite = fs_maxwrite;
    ops->fs_maxlink = fs_maxlink;
    ops->fs_maxnamelen = fs_maxnamelen;
    ops->fs_maxpathlen = fs_maxpathlen;
    ops->fs_lease_time = fs_lease_time;
    ops->fs_acl_support = fs_acl_support;
    ops->fs_supported_attrs = fs_supported_attrs;
    ops->fs_umask = fs_umask;
    ops->fs_xattr_access_rights = fs_xattr_access_rights;
#ifdef QUOTA_SUPPORT
    ops->get_quota = get_quota;
    ops->set_quota = set_quota;
#endif
}
