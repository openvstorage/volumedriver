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

#include "FileSystemTestSetup.h"

#include <youtils/Assert.h>
#include <youtils/LockedArakoon.h>
#include <youtils/System.h>

#include <volumedriver/metadata-server/Manager.h>
#include <volumedriver/VolumeDriverParameters.h>

#include <volumedriver/test/MDSTestSetup.h>
#include <volumedriver/test/MetaDataStoreTestSetup.h>

#include <filedriver/FileDriverParameters.h>

#include <filesystem/ClusterRegistry.h>
#include <filesystem/FileSystem.h>
#include <filesystem/FileSystemParameters.h>
#include <filesystem/VirtualDiskFormatVmdk.h>

namespace volumedriverfstest
{

namespace ara = arakoon;
namespace be = backend;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace mds = metadata_server;
namespace vd = volumedriver;
namespace vdt = volumedrivertest;
namespace vfs = volumedriverfs;
namespace yt = youtils;
namespace ytt = youtilstest;

using namespace std::literals::string_literals;

std::unique_ptr<vfs::VirtualDiskFormat>
FileSystemTestSetup::vdisk_format_(new vfs::VirtualDiskFormatVmdk());

std::string
FileSystemTestSetup::address_("127.0.0.1");

vd::FailOverCacheTransport
FileSystemTestSetup::failovercache_transport_(vd::FailOverCacheTransport::TCP);

FileSystemTestSetup::FileSystemTestSetup(const FileSystemTestSetupParameters& params)
    : ytt::TestBase()
    , be::BackendTestSetup()
    , test_name_(params.name_)
    , topdir_(getTempPath(test_name_))
    , configuration_(topdir_ / "configuration")
    , scache_size_(yt::DimensionedValue(params.scocache_size_).getBytes())
    , scache_trigger_gap_(yt::DimensionedValue(params.scocache_trigger_gap_).getBytes())
    , scache_backoff_gap_(yt::DimensionedValue(params.scocache_backoff_gap_).getBytes())
    , scache_clean_interval_(params.scocache_clean_interval_)
    , open_scos_per_volume_(params.open_scos_per_volume_)
    , dstore_throttle_usecs_(params.datastore_throttle_usecs_)
    , foc_throttle_usecs_(params.failovercache_throttle_usecs_)
    , num_threads_(params.num_threads_)
    , num_scos_in_tlog_(params.scos_per_tlog_)
    , backend_sync_timeout_ms_(params.backend_sync_timeout_ms_)
    , migrate_timeout_ms_(params.migrate_timeout_ms_)
    , redirect_timeout_ms_(params.redirect_timeout_ms_)
    , redirect_retries_(params.redirect_retries_)
    , fdriver_namespace_("ovs-fdnspc-fstest-"s + yt::UUID().str())
    , arakoon_test_setup_(std::make_shared<ara::ArakoonTestSetup>(topdir_ / "arakoon"))
    , client_(vrouter_cluster_id(),
              {{address(), local_config().xmlrpc_port}} )
{
    EXPECT_LE(scache_trigger_gap_, scache_backoff_gap_) <<
        "invalid trigger gap + backoff gap for the SCO cache specified, fix your test!";
    EXPECT_LT(scache_backoff_gap_, scache_size_) <<
        "invalid backoff gap specified for the SCO cache, fix your test!";
}

FileSystemTestSetup::~FileSystemTestSetup()
{
    // empty destructor to allow std::unique_ptr<ForwardDeclaredType> members
}

uint16_t
FileSystemTestSetup::port_base()
{
    const uint16_t def = 12321;
    return yt::System::get_env_with_default("VFS_PORT_BASE", def);
}

void
FileSystemTestSetup::start_failovercache_for_local_node()
{
    local_foc_helper_ =
        std::make_unique<FailOverCacheTestHelper>(failovercache_dir(remote_dir(topdir_)),
                                                  remote_config().host,
                                                  remote_config().failovercache_port,
                                                  failovercache_transport());
}

void
FileSystemTestSetup::stop_failovercache_for_local_node()
{
    local_foc_helper_.reset();
}

void
FileSystemTestSetup::start_failovercache_for_remote_node()
{
    remote_foc_helper_ =
        std::make_unique<FailOverCacheTestHelper>(failovercache_dir(topdir_),
                                                  local_config().host,
                                                  local_config().failovercache_port,
                                                  failovercache_transport());
}

void
FileSystemTestSetup::stop_failovercache_for_remote_node()
{
    remote_foc_helper_.reset();
}

void
FileSystemTestSetup::put_cluster_node_configs_in_registry(const bpt::ptree& pt)
{
    const ara::ClusterID
        ara_cluster_id(PARAMETER_VALUE_FROM_PROPERTY_TREE(vregistry_arakoon_cluster_id,
                                                          pt));
    const std::vector<ara::ArakoonNodeConfig>
        ara_node_configs(PARAMETER_VALUE_FROM_PROPERTY_TREE(vregistry_arakoon_cluster_nodes,
                                                            pt));
    auto arakoon(std::make_shared<yt::LockedArakoon>(ara_cluster_id,
                                                     ara_node_configs));
    const vfs::ClusterId
        vrouter_cluster_id(PARAMETER_VALUE_FROM_PROPERTY_TREE(vrouter_cluster_id,
                                                              pt));

    vfs::ClusterRegistry registry(vrouter_cluster_id,
                                  arakoon);

    registry.set_node_configs(cluster_node_configs());
}

void
FileSystemTestSetup::setup_clustercache_device(const fs::path& device,
                                               uint64_t size) const
{
    int fd = ::open(device.string().c_str(),
                    O_RDWR| O_CREAT | O_EXCL,
                    S_IWUSR | S_IRUSR);
    if(fd < 0)
    {
        LOG_ERROR("Couldn't open file " << device << ", " << strerror(errno));
        throw fungi::IOException("Couldn't open file", device.string().c_str());
    }

    BOOST_SCOPE_EXIT((fd))
    {
        ::close(fd);
    }
    BOOST_SCOPE_EXIT_END;

    int ret = ::posix_fallocate(fd, 0, size);
    if (ret != 0)
    {
        LOG_ERROR("Could not allocate file " << device << " with size " << size << ", " << strerror(ret));
        throw fungi::IOException("Could not allocate file",
                                 device.string().c_str(),
                                 ret);
    }

    ::fchmod(fd, 00600);
}

void
FileSystemTestSetup::maybe_setup_clustercache_device(const fs::path& device,
                                                     uint64_t size) const
{
    if (not fs::exists(device))
    {
        setup_clustercache_device(device, size);
    }
    else
    {
        ASSERT_EQ(size, fs::file_size(device));
    }
}

bpt::ptree&
FileSystemTestSetup::make_registry_config_(bpt::ptree& pt) const
{
    VERIFY(arakoon_test_setup_ != nullptr);

    const std::string cluster_id(arakoon_test_setup_->clusterID().str());
    ip::PARAMETER_TYPE(vregistry_arakoon_cluster_id)(cluster_id).persist(pt);

    const auto ara_node_list(arakoon_test_setup_->node_configs());
    const ip::PARAMETER_TYPE(vregistry_arakoon_cluster_nodes)::ValueType
        ara_node_vec(ara_node_list.begin(),
                     ara_node_list.end());
    ip::PARAMETER_TYPE(vregistry_arakoon_cluster_nodes)(ara_node_vec).persist(pt);

    return pt;
}

bpt::ptree&
FileSystemTestSetup::make_mdstore_config_(bpt::ptree& pt) const
{
    VERIFY(mdstore_test_setup_ != nullptr);

    const ip::PARAMETER_TYPE(fs_metadata_backend_type)
        backend_type(mdstore_test_setup_->backend_type_);
    backend_type.persist(pt);

    switch (backend_type.value())
    {
    case vd::MetaDataBackendType::Arakoon:
        {
            const std::string cluster_id(arakoon_test_setup_->clusterID().str());
            ip::PARAMETER_TYPE(fs_metadata_backend_arakoon_cluster_id)(cluster_id).persist(pt);

            const auto ara_nodel(arakoon_test_setup_->node_configs());
            const ip::PARAMETER_TYPE(fs_metadata_backend_arakoon_cluster_nodes)::ValueType
                ara_nodev(ara_nodel.begin(),
                          ara_nodel.end());
            ip::PARAMETER_TYPE(fs_metadata_backend_arakoon_cluster_nodes)(ara_nodev).persist(pt);
            break;
        }
    case vd::MetaDataBackendType::MDS:
        {
            VERIFY(mds_server_config_ != nullptr);
            const ip::PARAMETER_TYPE(fs_metadata_backend_mds_nodes)::ValueType
                mds_nodev({ mds_server_config_->node_config });
            ip::PARAMETER_TYPE(fs_metadata_backend_mds_nodes)(mds_nodev).persist(pt);
            break;
        }
    case vd::MetaDataBackendType::RocksDB:
    case vd::MetaDataBackendType::TCBT:
        break;
    }

    return pt;
}

bpt::ptree&
FileSystemTestSetup::make_config_(bpt::ptree& pt,
                                  const fs::path& topdir,
                                  const vfs::NodeId& vrouter_id) const
{
    be::BackendTestSetup::backend_config().persist_internal(pt, ReportDefault::T);

    // scocache
    {
        vd::MountPointConfigs mp_configs;

        mp_configs.push_back(vd::MountPointConfig(scocache_mountpoint(topdir).string(),
                                                  scache_size_));

        ip::PARAMETER_TYPE(scocache_mount_points)(mp_configs).persist(pt);
        ip::PARAMETER_TYPE(trigger_gap)(yt::DimensionedValue(scache_trigger_gap_)).persist(pt);
        ip::PARAMETER_TYPE(backoff_gap)(yt::DimensionedValue(scache_backoff_gap_)).persist(pt);
    }

    // clustercache
    {
        std::vector<vd::MountPointConfig> kfgs;

        yt::DimensionedValue csize("20MiB");

        const fs::path kdev(clustercache_mountpoint(topdir));
        maybe_setup_clustercache_device(kdev, csize.getBytes());
        vd::MountPointConfig mp(kdev, csize.getBytes());
        kfgs.push_back(mp);

        ip::PARAMETER_TYPE(clustercache_mount_points)(kfgs).persist(pt);
        ip::PARAMETER_TYPE(read_cache_serialization_path)(clustercache_serialization_path(topdir).string()).persist(pt);
    }

    // volmanager
    {
        ip::PARAMETER_TYPE(tlog_path)(tlog_dir(topdir).string()).persist(pt);
        ip::PARAMETER_TYPE(metadata_path)(mdstore_dir(topdir).string()).persist(pt);
        ip::PARAMETER_TYPE(open_scos_per_volume)(open_scos_per_volume_).persist(pt);
        ip::PARAMETER_TYPE(datastore_throttle_usecs)(dstore_throttle_usecs_).persist(pt);
        ip::PARAMETER_TYPE(clean_interval)(scache_clean_interval_).persist(pt);
        ip::PARAMETER_TYPE(number_of_scos_in_tlog)(num_scos_in_tlog_).persist(pt);
        ip::PARAMETER_TYPE(debug_metadata_path)(dump_on_halt_dir(topdir).string()).persist(pt);

        // (backend)threadpool
        ip::PARAMETER_TYPE(num_threads)(num_threads_).persist(pt);
    }

    // metadata_server - we run it outside volmanager, hence empty ServerConfigs here.
    {
        const mds::ServerConfigs scfgs;
        mds_test_setup_->make_manager_config(pt,
                                             scfgs);
    }

    // distributed_lock_store
    {
        ip::PARAMETER_TYPE(dls_type)(vd::LockStoreType::Arakoon).persist(pt);
        ip::PARAMETER_TYPE(dls_arakoon_cluster_id)(arakoon_test_setup_->clusterID().str()).persist(pt);
        const auto node_configs(arakoon_test_setup_->node_configs());
        const ip::PARAMETER_TYPE(dls_arakoon_cluster_nodes)::ValueType
            node_configv(node_configs.begin(),
                         node_configs.end());

        ip::PARAMETER_TYPE(dls_arakoon_cluster_nodes)(node_configv).persist(pt);
    }
    // filedriver
    {
        ip::PARAMETER_TYPE(fd_cache_path)(fdriver_cache_dir(topdir).string()).persist(pt);
        ip::PARAMETER_TYPE(fd_namespace)(fdriver_namespace_.str()).persist(pt);
    }

    // filesystem
    {
        ip::PARAMETER_TYPE(fs_ignore_sync)(false).persist(pt);

        const std::string backend_dir(source_dir().string());
        ip::PARAMETER_TYPE(fs_virtual_disk_format)(vdisk_format_->name()).persist(pt);

        ip::PARAMETER_TYPE(fs_cache_dentries)(true).persist(pt);

        make_mdstore_config_(pt);
    }

    // volume_router
    {
        ip::PARAMETER_TYPE(vrouter_backend_sync_timeout_ms)(backend_sync_timeout_ms_).persist(pt);
        ip::PARAMETER_TYPE(vrouter_migrate_timeout_ms)(migrate_timeout_ms_).persist(pt);
        ip::PARAMETER_TYPE(vrouter_redirect_timeout_ms)(redirect_timeout_ms_).persist(pt);
        ip::PARAMETER_TYPE(vrouter_redirect_retries)(redirect_retries_).persist(pt);
        ip::PARAMETER_TYPE(vrouter_id)(vrouter_id).persist(pt);
    }

    // volume_router_cluster
    {
        ip::PARAMETER_TYPE(vrouter_cluster_id)(vrouter_cluster_id()).persist(pt);
    }

    //failovercache
    {
        ip::PARAMETER_TYPE(failovercache_path)(failovercache_dir(topdir).string()).persist(pt);
        ip::PARAMETER_TYPE(failovercache_transport)(failovercache_transport()).persist(pt);
    }

    // volume_registry
    make_registry_config_(pt);

    // event_publisher is not configured for now as it's entirely optional.

    return pt;
}

void
FileSystemTestSetup::make_dirs_(const fs::path& pfx) const
{
    fs::create_directories(source_dir());
    fs::create_directories(mount_dir(pfx));
    fs::create_directories(clustercache_dir(pfx));
    fs::create_directories(scocache_mountpoint(pfx));
    fs::create_directories(tlog_dir(pfx));
    fs::create_directories(mdstore_dir(pfx));
    fs::create_directories(dump_on_halt_dir(pfx));
    fs::create_directories(failovercache_dir(topdir_));
    fs::create_directories(fdriver_cache_dir(pfx));
}

void
FileSystemTestSetup::SetUp()
{
    Py_Initialize();

    fs::remove_all(topdir_);

    make_dirs_(topdir_);
    fs::create_directories(remote_dir(topdir_));

    ::setenv("VD_CONFIG_DIR", configuration_.string().c_str(), 1);

    initialize_connection_manager();

    arakoon_test_setup_->setUpArakoon();

    mds_test_setup_ = std::make_shared<vdt::MDSTestSetup>(mds_dir(topdir_));
    mds_manager_ = mds_test_setup_->make_manager(cm_);
    const mds::ServerConfigs scfgs(mds_manager_->server_configs());

    ASSERT_EQ(1U,
              scfgs.size());

    mds_server_config_ = std::make_unique<mds::ServerConfig>(scfgs[0]);
    mdstore_test_setup_ =
        std::make_unique<vdt::MetaDataStoreTestSetup>(arakoon_test_setup_,
                                                      mds_server_config_->node_config);

    start_failovercache_for_local_node();
}

void
FileSystemTestSetup::TearDown()
{
    bpt::ptree pt;

    // Fill the ptree first as it requires e.g. the mdstore_test_setup_ to be
    // still around.
    make_config_(pt,
                 topdir_,
                 local_config().vrouter_id);

    stop_failovercache_for_local_node();

    mdstore_test_setup_.reset();
    mds_server_config_.reset();
    mds_manager_.reset();
    mds_test_setup_ = nullptr;

    try
    {
        vfs::FileSystem::destroy(pt);
    }
    CATCH_STD_ALL_LOGLEVEL_IGNORE("Failed to clean up filesystem - resources might be leaked; manual intervention required",
                                  FATAL);

    arakoon_test_setup_->tearDownArakoon();

    uninitialize_connection_manager();

    fs::remove_all(topdir_);

    Py_Finalize();
}

fs::path
FileSystemTestSetup::make_volume_name(const std::string& s)
{
    const fs::path p(s + vdisk_format_->volume_suffix());
    VERIFY(vdisk_format_->is_volume_path(vfs::FrontendPath(p)));
    return p;
}

vd::VolumeConfig::MetaDataBackendConfigPtr
FileSystemTestSetup::make_metadata_backend_config()
{
    VERIFY(mdstore_test_setup_ != nullptr);
    return vd::VolumeConfig::MetaDataBackendConfigPtr(mdstore_test_setup_->make_config().release());
}

}
