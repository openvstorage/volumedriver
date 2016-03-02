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

#include <string.h>
#include <sys/types.h>
#include <thread>

#include "FileSystemWrapper.h"
#include "Common.h"
#include "TCPServer.h"

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
using namespace std::literals::string_literals;

namespace yt = youtils;
namespace vfs = volumedriverfs;

#if 0
uint64_t
group(const struct attrlist* attrlist)
{
    VERIFY(attrlist->mask bitand ATTR_GROUP);
    return attrlist->group;
}
#endif

const std::string logfile_key("VFS_FSAL_LOGFILE");
const std::string logsink_key("VFS_FSAL_LOGSINK");
const std::string loglevel_key("VFS_FSAL_LOGLEVEL");
const std::string logrotation_key("VFS_FSAL_LOGROTATION");
const std::string logdisable_key("VFS_FSAL_LOGDISABLE");

void
initialize_logging()
{
    static bool logging_initialized = false;
    static bool logging_disabled = yt::System::get_bool_from_env(logdisable_key,
                                                                 false);

    if(not logging_initialized)
    {
        logging_initialized = true;

        const std::string logfile(yt::System::get_env_with_default(logfile_key,
                                                                   ""s));

        const std::string logsink_default("./VolumeDriverGanesha.log");
        const std::string logsink(yt::System::get_env_with_default(logsink_key,
                                                                   logsink_default));

        const yt::Severity sev(yt::System::get_env_with_default(loglevel_key,
                                                                yt::Severity::info));

        const bool rotate = yt::System::get_bool_from_env(logrotation_key,
                                                          false);

        std::vector<std::string> sinks = { logsink };
        if (not logfile.empty())
        {
            sinks.push_back(logfile);
        }

        yt::Logger::setupLogging("ganesha_fsal",
                                 sinks,
                                 sev,
                                 rotate ? yt::LogRotation::T : yt::LogRotation::F);
    }

    if (logging_disabled && logging_initialized)
    {
        yt::Logger::disableLogging();
    }
}

/* We might want to take some away here. */
#define OVS_SUPPORTED_ATTRIBUTES (                                          \
                    ATTR_TYPE     | ATTR_SIZE     |                         \
                    ATTR_FSID     | ATTR_FILEID   |                         \
                    ATTR_MODE     | ATTR_NUMLINKS | ATTR_OWNER     |        \
                    ATTR_GROUP    | ATTR_ATIME    | ATTR_RAWDEV    |        \
                    ATTR_CTIME    | ATTR_MTIME    | ATTR_SPACEUSED |        \
                    ATTR_CHGTIME)

static struct fsal_staticfsinfo_t default_ovs_info =  {
    .maxfilesize = 0xFFFFFFFFFFFFFFFFLL,	/* (64bits) */
    .maxlink = _POSIX_LINK_MAX,
    .maxnamelen = 1024,
    .maxpathlen = 1024,
    .no_trunc = true,
    .chown_restricted = true,
    .case_insensitive = false,
    .case_preserving = true,
    .link_support = false,
    .symlink_support = false,
    .lock_support = true,
    .lock_support_owner = true,
    .lock_support_async_block = true,
    .named_attr = false,
    .unique_handles = true,
    .lease_time = {10, 0},
    .acl_support = FSAL_ACLSUPPORT_ALLOW,
    .cansettime = true,
    .homogenous = true,
    .supported_attrs = OVS_SUPPORTED_ATTRIBUTES,
    .maxread = 0,
    .maxwrite = 0,
    .umask = 0,
    .auth_exportpath_xdev = false,
    .xattr_access_rights = 0400,	/* root=RW, owner=R */
    .accesscheck_support = false,
    .share_support = false,
    .share_support_owner= false,
    .delegations = 0,
    .pnfs_file = 0,
    .reopen_method = false,
    .fsal_trace = false,
};

struct this_export_params {
    char *ovs_config_location;
    char *ovs_config_file; // deprecated, will go away after a transition period
    uint16_t ovs_discovery_port;
};

static struct config_item export_params[4];
static struct config_block export_param;

/* @brief Initialize the configuration
 *
 * Given the root of the Ganesha configuration structure, initialize
 * the FSAL parameters.
 *
 * @param[in] fsal_hdl      The FSAL module
 * @param[in] config_struct Parsed ganesha configuration file
 *
 * @return FSAL status.
 */
static fsal_status_t init_config(struct fsal_module *fsal_hdl,
                                 config_file_t /*config_struct*/)
{
    struct this_fsal_module *module_me =
        container_of(fsal_hdl, struct this_fsal_module, fsal);

    /* Get a copy of the defaults */
    module_me->fs_info = default_ovs_info;
    display_fsinfo(&module_me->fs_info);
    try
    {
        initialize_logging();
    }
    CATCH_STD_ALL_EWHAT({
            std::stringstream ss;
            ss << EWHAT;
            LogMajor(COMPONENT_FSAL,
                     const_cast<char*>("VFS FSAL: failed to initialize logging: %s"),
                     ss.str().c_str());
            return fsalstat(ERR_FSAL_BAD_INIT, 0);
    });

    return fsalstat(ERR_FSAL_NO_ERROR, 0);
}

/**
 * @brief Create a new export under this FSAL
 */
static fsal_status_t create_export(struct fsal_module *fsal_hdl,
                                   void *parse_node,
                                   const struct fsal_up_vector *up_ops)
{
    int rc;
    struct config_error_type err_type;
    /* The status code to return */
    fsal_status_t status = { ERR_FSAL_NO_ERROR, 0 };
    /* Internal export object */
    this_fsal_export *filesystem_export = NULL;
    /* OVS FSAL argument object */
    struct this_export_params params = {
        .ovs_config_location = NULL,
        .ovs_config_file = NULL,
        .ovs_discovery_port = 0,
    };

    export_params[0].name = const_cast<char*>("name");
    export_params[0].type = CONFIG_NULL;

    export_params[1].name = const_cast<char*>("ovs_config");
    export_params[1].type = CONFIG_STRING;
    export_params[1].u.str.minsize = 0;
    export_params[1].u.str.maxsize = MAXPATHLEN;
    /* Do we have a predefined path for the volumedriver config file? */
    export_params[1].u.str.def = nullptr;
    export_params[1].off = offsetof(struct this_export_params, ovs_config_location);

    // deprecated, will go away after a transition period
    export_params[2].name = const_cast<char*>("ovs_config_file");
    export_params[2].type = CONFIG_PATH;
    export_params[2].u.str.minsize = 0;
    export_params[2].u.str.maxsize = MAXPATHLEN;
    /* Do we have a predefined path for the volumedriver config file? */
    export_params[2].u.str.def = nullptr;
    export_params[2].off = offsetof(struct this_export_params, ovs_config_file);

    export_params[3].name = const_cast<char*>("ovs_discovery_port");
    export_params[3].type = CONFIG_UINT16;
    export_params[3].u.ui16.minval = 1024;
    export_params[3].u.ui16.maxval = UINT16_MAX;
    /* default discovery port */
    export_params[3].u.ui16.def = 14147;
    export_params[3].off = offsetof(struct this_export_params, ovs_discovery_port);

    export_params[4].name = NULL;
    export_params[4].type = CONFIG_NULL;

    export_param.dbus_interface_name =
        const_cast<char*>("org.ganesha.nfsd.config.fsal.ovs-export%d");
    export_param.blk_desc.name = const_cast<char*>("FSAL");
    export_param.blk_desc.type = CONFIG_BLOCK;
    export_param.blk_desc.u.blk.init = noop_conf_init;
    export_param.blk_desc.u.blk.params = export_params;
    export_param.blk_desc.u.blk.commit = noop_conf_commit;

    LogDebug(COMPONENT_FSAL,
             const_cast<char*>("In args: export path = %s"),
             op_ctx->export_->fullpath);

    filesystem_export = reinterpret_cast<struct this_fsal_export *>(
            gsh_calloc(1, sizeof(struct this_fsal_export)));

    if (filesystem_export == NULL)
    {
        status.major = ERR_FSAL_NOMEM;
        LogCrit(COMPONENT_FSAL,
                const_cast<char*>("Unable to allocate export object. Export: %s"),
                op_ctx->export_->fullpath);
        return status;
    }

    if (fsal_export_init(&filesystem_export->export_) != 0 )
    {
        status.major = ERR_FSAL_NOMEM;
        LogCrit(COMPONENT_FSAL,
                const_cast<char*>("Unable to allocate export ops vectors. Export: %s"),
                op_ctx->export_->fullpath);
        gsh_free(filesystem_export);
        return status;
    }

    export_ops_init(filesystem_export->export_.ops);
    handle_ops_init(filesystem_export->export_.obj_ops);
    filesystem_export->export_.up_ops = up_ops;

    /* Parse and load OVS FSAL configuration options */
    rc = load_config_from_node(parse_node,
                               &export_param,
                               &params,
                               true,
                               &err_type);

    if (rc != 0)
    {
        LogCrit(COMPONENT_FSAL,
                const_cast<char*>("Incorrect or missing parameters for export %s"),
                op_ctx->export_->fullpath);
        status.major = ERR_FSAL_INVAL;
        gsh_free(filesystem_export);
        return status;
    }

    LogDebug(COMPONENT_FSAL,
             const_cast<char*>("OVS volumedriverfs configuration file: %s"),
             params.ovs_config_location);

    const std::string config_location(params.ovs_config_location ?
                                      params.ovs_config_location :
                                      params.ovs_config_file);

    /* Instantiate new FileSystemWrapper */
    filesystem_export->wrapper_ = new FileSystemWrapper(op_ctx->export_->fullpath,
                                                        config_location);

    /* Start OVS discovery server
     * We provide one export per ganesha instance so there is no
     * problem instantiating the discovery socket server here.
     */
    try
    {
        LogDebug(COMPONENT_FSAL,
                 const_cast<char*>("OVS discovery port: %d"),
                 params.ovs_discovery_port);

        const uint16_t server_port(params.ovs_discovery_port);
        int thread_pool_size = std::thread::hardware_concurrency();
        filesystem_export->server_ = new ovs_discovery_server(server_port,
                                                              thread_pool_size,
                                                              filesystem_export->wrapper_);
        filesystem_export->server_->run();
    }
    catch(std::exception& e)
    {
        status.major = ERR_FSAL_SERVERFAULT;
        LogCrit(COMPONENT_FSAL,
                const_cast<char*>("Unable to start ovs discovery tcp server: %s"),
                e.what());
        gsh_free(filesystem_export);
        return status;
    }

    if ((rc = fsal_attach_export(fsal_hdl,
                    &filesystem_export->export_.exports)) != 0)
    {
        status.major = ERR_FSAL_SERVERFAULT;
        LogCrit(COMPONENT_FSAL,
                const_cast<char*>("Unable to attach export. Export %s"),
                op_ctx->export_->fullpath);
        gsh_free(filesystem_export);
        return status;
    }

    filesystem_export->export_.fsal = fsal_hdl;

    op_ctx->fsal_export = &filesystem_export->export_;
    LogDebug(COMPONENT_FSAL,
             const_cast<char*>("FSAL create_export finished successfully."));

    filesystem_export->wrapper_->up_and_running(op_ctx->export_->fullpath);

    return status;
}

/**
 * Local copy of the handle for this module.
 */
static struct this_fsal_module OVS;

/**
 * Name of this module
 */
static const char OVS_name[] = "OVS";

/**
 * @brief Initialize and register the FSAL
 *
 * This function initializes the FSAL module handle, being
 * called before any configuration or mounting. It exists
 * solely to produce a properly constructed FSAL module handle.
 * Currently, there is no private, per-module data or initialization.
 */
MODULE_INIT void
init(void)
{
    struct fsal_module *myself = &OVS.fsal;

    if (register_fsal(myself, OVS_name,
                      FSAL_MAJOR_VERSION,
                      FSAL_MINOR_VERSION,
                      FSAL_ID_OVS_FILESYSTEM) != 0)
    {
        LogCrit(COMPONENT_FSAL,
                const_cast<char*>("OVS FSAL module failed to register."));
        return;
    }

    /* set up module operations */
    myself->ops->create_export = create_export;
    myself->ops->init_config = init_config;

    LogDebug(COMPONENT_FSAL,
             const_cast<char*>("FSAL OVS initialized."));
}

/**
 * @brief Release FSAL resources
 *
 * This function unregisters the FSAL.
 * No resources to release for the underlying filesystem layer
 * in the per-FSAL level.
 */
MODULE_FINI void
unload(void)
{
    if (unregister_fsal(&OVS.fsal) != 0)
    {
        LogCrit(COMPONENT_FSAL,
                const_cast<char*>("OVS FSAL unable to unload. Aborting..."));
        abort();
    }

    LogDebug(COMPONENT_FSAL,
             const_cast<char*>("FSAL OVS unloaded"));
}

} //namespace
