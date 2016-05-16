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

#include "FileSystemTestBase.h"

#include <youtils/UpdateReport.h>

#include <volumedriver/Api.h>
#include <volumedriver/metadata-server/Manager.h>

#include "../../volumedriver/test/MDSTestSetup.h"
#include "../../volumedriver/test/MetaDataStoreTestSetup.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/python/extract.hpp>

namespace volumedriverfstest
{

namespace bpt = boost::property_tree;
namespace bpy = boost::python;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace mds = metadata_server;
namespace vd = volumedriver;
namespace vfs = volumedriverfs;
namespace yt = youtils;

class VolumeTest
    : public FileSystemTestBase
{
protected:
    VolumeTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("VolumeTest"))
    {}

    void
    test_suffix(const std::string sfx)
    {
        vfs::FileSystem::verify_volume_suffix_(sfx);
    }

    DECLARE_LOGGER("VolumeTest");
};

#define LOCKVD()                                        \
    fungi::ScopedLock ag__(api::getManagementMutex())

TEST_F(VolumeTest, volume_suffix)
{
#define OK(sfx)                                  \
    EXPECT_NO_THROW(test_suffix(sfx));

#define NOK(sfx)                                        \
    EXPECT_THROW(test_suffix(sfx), std::exception)

    NOK("");
    NOK("foo");
    NOK("/");
    NOK("/foo");
    NOK("./");
    NOK("./foo");
    NOK(".");
    NOK(".foo/");

    OK(".."); // do we really want to allow that?
    OK(".foo");
    OK("..foo");

#undef NOK
#undef OK
}

TEST_F(VolumeTest, create_and_destroy)
{
    const vfs::FrontendPath fname(make_volume_name("/volume"));

    verify_absence(fname);

    const vfs::ObjectId vname(create_file(fname));
    verify_registration(vname, local_node_id());

    {
        LOCKVD();

        std::list<vd::VolumeId> l;
        api::getVolumeList(l);
        ASSERT_EQ(1U, l.size());
        ASSERT_TRUE(vname.str() == l.front().str());
        ASSERT_NO_THROW(api::getVolumePointer(l.front()));
    }

    check_stat(fname, 0);

    destroy_volume(fname);

    {
        LOCKVD();

        std::list<vd::VolumeId> l;
        api::getVolumeList(l);
        ASSERT_TRUE(l.empty());
        ASSERT_THROW(api::getVolumePointer(vd::VolumeId(vname.str())), std::exception);
    }

    verify_absence(fname);
    ASSERT_FALSE(is_registered(vname));
}

TEST_F(VolumeTest, uuid_create_and_destroy)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath vname(make_volume_name("/volume"));

    verify_absence(vname);

    const vfs::ObjectId volume_id(create_file(root_id,
                                              vname.str().substr(1)));
    verify_registration(volume_id, local_node_id());

    {
        LOCKVD();

        std::list<vd::VolumeId> l;
        api::getVolumeList(l);
        ASSERT_EQ(1U, l.size());
        ASSERT_TRUE(volume_id.str() == l.front().str());
        ASSERT_NO_THROW(api::getVolumePointer(l.front()));
    }

    check_stat(volume_id, 0);

    destroy_volume(root_id,
                   vname.str().substr(1));

    {
        LOCKVD();

        std::list<vd::VolumeId> l;
        api::getVolumeList(l);
        ASSERT_TRUE(l.empty());
        ASSERT_THROW(api::getVolumePointer(vd::VolumeId(volume_id.str())),
                     std::exception);
    }

    verify_absence(volume_id);
    ASSERT_FALSE(is_registered(volume_id));
}

TEST_F(VolumeTest, recreation)
{
    const vfs::FrontendPath vname(make_volume_name("/volume"));

    verify_absence(vname);
    const vfs::ObjectId id(create_file(vname));

    verify_registration(id, local_node_id());
    check_stat(vname, 0);

    EXPECT_EQ(-EEXIST,
              mknod(vname,
                    S_IFREG bitor S_IWUSR bitor S_IRUSR,
                    0));

    check_stat(vname, 0);
    verify_registration(id, local_node_id());
}

TEST_F(VolumeTest, uuid_recreation)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath vname(make_volume_name("/volume"));

    verify_absence(vname);
    const vfs::ObjectId volume_id(create_file(root_id,
                                              vname.str().substr(1)));

    verify_registration(volume_id, local_node_id());
    check_stat(volume_id, 0);

    EXPECT_EQ(-EEXIST, mknod(root_id,
                             vname.str().substr(1),
                             S_IFREG bitor S_IWUSR bitor S_IRUSR));

    check_stat(volume_id, 0);
    verify_registration(volume_id, local_node_id());
}

TEST_F(VolumeTest, open_close)
{
    const vfs::FrontendPath vname(make_volume_name("/volume"));
    create_file(vname);

    vfs::Handle::Ptr h;

    EXPECT_EQ(0, open(vname, h, O_RDONLY));
    EXPECT_EQ(0, release(vname, std::move(h)));
}

TEST_F(VolumeTest, uuid_open_close)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const std::string fname(make_volume_name("volume").string());

    const vfs::ObjectId volume_id(create_file(root_id,
                                              fname));
    const mode_t openflags = O_RDONLY;

    vfs::Handle::Ptr h;

    EXPECT_EQ(0, open(volume_id,
                       h,
                       openflags));
    EXPECT_EQ(0, release(std::move(h)));
}

TEST_F(VolumeTest, rename)
{
    const vfs::FrontendPath p(make_volume_name("/some-volume"));

    verify_absence(p);

    const auto vname(create_file(p));
    check_stat(p, 0);
    verify_registration(vname, local_node_id());

    const vfs::FrontendPath q(make_volume_name("/some-volume-renamed"));
    verify_absence(q);

    EXPECT_EQ(-EPERM, rename(p, q));
    verify_absence(q);
    check_stat(p, 0);

    verify_registration(vname, local_node_id());
}

TEST_F(VolumeTest, uuid_rename)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath p(make_volume_name("/some-volume"));

    verify_absence(p);

    const auto volume_id(create_file(root_id,
                                     p.str().substr(1)));
    check_stat(volume_id, 0);
    verify_registration(volume_id, local_node_id());

    const vfs::FrontendPath q(make_volume_name("/some-volume-renamed"));
    verify_absence(q);

    EXPECT_EQ(-EPERM, rename(root_id,
                             p.str().substr(1),
                             root_id,
                             q.str().substr(1)));
    verify_absence(q);
    check_stat(volume_id, 0);

    verify_registration(volume_id, local_node_id());
}

TEST_F(VolumeTest, resize)
{
    const vfs::FrontendPath fname(make_volume_name("/volume"));
    const vfs::ObjectId vname(create_file(fname));

    vd::WeakVolumePtr v;

    {
        LOCKVD();
        v = api::getVolumePointer(vd::VolumeId(vname.str()));
    }

    EXPECT_EQ(0U, api::GetSize(v));
    check_stat(fname, 0);

    EXPECT_EQ(0, truncate(fname, 0));
    EXPECT_EQ(0U, api::GetSize(v));
    check_stat(fname, 0);

    const uint64_t csize = get_cluster_size(vname);
    EXPECT_EQ(0, truncate(fname, csize));
    EXPECT_EQ(csize, api::GetSize(v));
    check_stat(fname, csize);

    EXPECT_GT(0, truncate(fname, 0));

    EXPECT_EQ(csize, api::GetSize(v));
    check_stat(fname, csize);
}

TEST_F(VolumeTest, uuid_resize)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));

    const auto volume_id(create_file(root_id,
                                     make_volume_name("volume").string()));

    vd::WeakVolumePtr v;

    {
        LOCKVD();
        v = api::getVolumePointer(vd::VolumeId(volume_id.str()));
    }

    EXPECT_EQ(0U, api::GetSize(v));
    check_stat(volume_id, 0);

    EXPECT_EQ(0, truncate(volume_id, 0));
    EXPECT_EQ(0U, api::GetSize(v));
    check_stat(volume_id, 0);

    const uint64_t csize = get_cluster_size(volume_id);
    EXPECT_EQ(0, truncate(volume_id, csize));
    EXPECT_EQ(csize, api::GetSize(v));
    check_stat(volume_id, csize);

    EXPECT_GT(0, truncate(volume_id, 0));

    EXPECT_EQ(csize, api::GetSize(v));
    check_stat(volume_id, csize);
}

TEST_F(VolumeTest, read_empty_aligned)
{
    const uint64_t vsize = 1 << 20;

    const vfs::FrontendPath fname(make_volume_name("/volume"));
    const vfs::ObjectId vname(create_file(fname, vsize));
    const uint64_t csize = get_cluster_size(vname);

    const std::vector<char> ref(csize, 0);

    for (size_t i = 0; i < vsize; i += csize)
    {
        std::vector<char> buf(csize);
        EXPECT_EQ(static_cast<ssize_t>(buf.size()),
                  read_from_file(fname, buf.data(), buf.size(), i));
        EXPECT_TRUE(ref == buf);
    }
    // Reads beyond the end return nothing
    for(size_t i = vsize; i < 2*vsize; i += csize)
    {
        std::vector<char> buf(csize);
        EXPECT_EQ(0, read_from_file(fname, buf.data(), buf.size(), i));
    }
    // Reads across the end return data to the end
    {
        std::vector<char> buf(2*csize);
        EXPECT_EQ(static_cast<ssize_t>(csize),
                  read_from_file(fname, buf.data(), buf.size(), vsize-csize));
        for(size_t i = 0; i < csize; ++i)
        {
            EXPECT_EQ(ref[i], buf[i]);
        }
    }
}

TEST_F(VolumeTest, uuid_read_empty_aligned)
{
    const uint64_t vsize = 1 << 20;

    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const auto volume_id(create_file(root_id,
                                     make_volume_name("volume").string(),
                                     vsize));
    const uint64_t csize = get_cluster_size(volume_id);

    const std::vector<char> ref(csize, 0);

    for (size_t i = 0; i < vsize; i += csize)
    {
        std::vector<char> buf(csize);
        EXPECT_EQ(static_cast<ssize_t>(buf.size()),
                  read_from_file(volume_id, buf.data(), buf.size(), i));
        EXPECT_TRUE(ref == buf);
    }
    // Reads beyond the end return nothing
    for(size_t i = vsize; i < 2*vsize; i += csize)
    {
        std::vector<char> buf(csize);
        EXPECT_EQ(0, read_from_file(volume_id, buf.data(), buf.size(), i));
    }
    // Reads across the end return data to the end
    {
        std::vector<char> buf(2*csize);
        EXPECT_EQ(static_cast<ssize_t>(csize),
                  read_from_file(volume_id, buf.data(), buf.size(), vsize-csize));
        for(size_t i = 0; i < csize; ++i)
        {
            EXPECT_EQ(ref[i], buf[i]);
        }
    }
}

TEST_F(VolumeTest, read_write_aligned)
{
    const uint64_t vsize = 10 << 20;

    const vfs::FrontendPath fname(make_volume_name("/volume"));
    const vfs::ObjectId vname(create_file(fname, vsize));
    const uint64_t csize = get_cluster_size(vname);

    const std::string pattern("Herr Bar");
    for (uint64_t i = 0; i < vsize; i += csize)
    {
        write_to_file(fname, pattern, csize, i);
    }

    for (uint64_t i = 0; i < vsize; i += csize)
    {
        check_file(fname, pattern, csize, i);
    }
    // Reads across the end return data to the end
    {
        std::vector<char> buf(2*csize);
        EXPECT_EQ(static_cast<ssize_t>(csize),
                  read_from_file(fname, buf.data(), buf.size(), vsize-csize));
        for (size_t i = 0; i < csize; ++i)
        {
            EXPECT_EQ(pattern[i % pattern.size()], buf[i]) << "mismatch at offset " << i;
        }
    }
}

TEST_F(VolumeTest, uuid_read_write_aligned)
{
    const uint64_t vsize = 10 << 20;

    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const auto volume_id(create_file(root_id,
                                     make_volume_name("volume").string(),
                                     vsize));

    const uint64_t csize = get_cluster_size(volume_id);

    const std::string pattern("Herr Bar");
    for (uint64_t i = 0; i < vsize; i += csize)
    {
        write_to_file(volume_id, pattern, csize, i);
    }

    for (uint64_t i = 0; i < vsize; i += csize)
    {
        check_file(volume_id, pattern, csize, i);
    }
    // Reads across the end return data to the end
    {
        std::vector<char> buf(2*csize);
        EXPECT_EQ(static_cast<ssize_t>(csize),
                  read_from_file(volume_id, buf.data(), buf.size(), vsize-csize));
        for (size_t i = 0; i < csize; ++i)
        {
            EXPECT_EQ(pattern[i % pattern.size()], buf[i]) << "mismatch at offset " << i;
        }
    }
}

TEST_F(VolumeTest, read_write_subcluster)
{
    const uint64_t vsize = 1 << 20;

    const vfs::FrontendPath fname(make_volume_name("/volume"));
    const vfs::ObjectId vname(create_file(fname, vsize));

    const std::string pattern("Frau Wav");
    const off_t off = 3;

    write_to_file(fname, pattern, pattern.size(), off);
    check_file(fname, pattern, pattern.size(), off);
}

TEST_F(VolumeTest, uuid_read_write_subcluster)
{
    const uint64_t vsize = 10 << 20;

    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const auto volume_id(create_file(root_id,
                                     make_volume_name("volume").string(),
                                     vsize));

    const std::string pattern("Frau Wav");
    const off_t off = 3;

    write_to_file(volume_id, pattern, pattern.size(), off);
    check_file(volume_id, pattern, pattern.size(), off);
}

TEST_F(VolumeTest, read_write_unaligned)
{
    const uint64_t vsize = 1 << 20;

    const vfs::FrontendPath fname(make_volume_name("/volume"));
    const vfs::ObjectId vname(create_file(fname, vsize));
    const uint64_t csize = get_cluster_size(vname);

    const std::string pattern("Ted");
    const off_t off = 3;

    write_to_file(fname, pattern, csize + 1, off);
    check_file(fname, pattern, csize + 1, off);
    // Reads across the end return data to the end

    {
        uint64_t half_a_c = csize/2;
        EXPECT_LT(half_a_c, csize);
        EXPECT_EQ(2*half_a_c, csize);
        write_to_file(fname, pattern, half_a_c, vsize-half_a_c);

        std::vector<char> buf(csize);
        EXPECT_EQ(static_cast<ssize_t>(half_a_c),
                  read_from_file(fname, buf.data(), buf.size(), vsize-half_a_c));
        for (size_t i = 0; i < half_a_c; ++i)
        {
            EXPECT_EQ(pattern[i % pattern.size()], buf[i]) << "mismatch at offset " << i;
        }
    }
}

TEST_F(VolumeTest, uuid_read_write_unaligned)
{
    const uint64_t vsize = 10 << 20;

    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const auto volume_id(create_file(root_id,
                                     make_volume_name("volume").string(),
                                     vsize));
    const uint64_t csize = get_cluster_size(volume_id);

    const std::string pattern("Ted");
    const off_t off = 3;

    write_to_file(volume_id, pattern, csize + 1, off);
    check_file(volume_id, pattern, csize + 1, off);
    // Reads across the end return data to the end

    {
        uint64_t half_a_c = csize/2;
        EXPECT_LT(half_a_c, csize);
        EXPECT_EQ(2*half_a_c, csize);
        write_to_file(volume_id, pattern, half_a_c, vsize-half_a_c);

        std::vector<char> buf(csize);
        EXPECT_EQ(static_cast<ssize_t>(half_a_c),
                  read_from_file(volume_id, buf.data(), buf.size(), vsize-half_a_c));
        for (size_t i = 0; i < half_a_c; ++i)
        {
            EXPECT_EQ(pattern[i % pattern.size()], buf[i]) << "mismatch at offset " << i;
        }
    }
}

TEST_F(VolumeTest, lost_registration)
{
    const uint64_t vsize = 1 << 20;
    const vfs::FrontendPath fname(make_volume_name("/volume"));
    const vfs::ObjectId vname(create_file(fname, vsize));

    fs_->object_router().object_registry()->unregister(vname);

    const std::string pattern("blah");
    const off_t off = 0;

    EXPECT_EQ(-ENOENT, write_to_file(fname, pattern, pattern.size(), off));
}

TEST_F(VolumeTest, uuid_lost_registration)
{
    const uint64_t vsize = 10 << 20;

    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const auto volume_id(create_file(root_id,
                                     make_volume_name("volume").string(),
                                     vsize));

    fs_->object_router().object_registry()->unregister(volume_id);

    const std::string pattern("blah");
    const off_t off = 0;

    EXPECT_EQ(-ENOENT, write_to_file(volume_id, pattern, pattern.size(), off));
}

TEST_F(VolumeTest, clone)
{
    const vfs::FrontendPath parent_path(make_volume_name("/volume"));

    struct stat st;
    EXPECT_EQ(-ENOENT, getattr(parent_path, st));

    const uint64_t vsize = 1 << 20;

    const vfs::ObjectId parent_id(create_file(parent_path, vsize));

    const std::string pattern("Konstantin");
    const off_t off = 10 * get_cluster_size(parent_id);
    const uint64_t size = 30 * get_cluster_size(parent_id);

    write_to_file(parent_path, pattern, size, off);
    check_file(parent_path, pattern, size, off);

    check_stat(parent_path, vsize);
    client_.set_volume_as_template(parent_id);

    const vfs::FrontendPath clone_path(make_volume_name("/the_clone"));

    const vfs::ObjectId
        clone_id(fs_->create_clone(clone_path,
                                   make_metadata_backend_config(),
                                   vd::VolumeId(parent_id.str()),
                                   boost::none).str());
    verify_presence(clone_path);

    {
        struct stat st;
        memset(&st, 0x0, sizeof(st));

        ASSERT_EQ(0, getattr(clone_path, st));
        ASSERT_GT(st.st_size, 0);
    }

    const vfs::FrontendPath vpath(clone_path_to_volume_path(clone_path));

    //TODO [BDV] change to a diff-> simpler and stronger check
    check_stat(vpath, vsize);
    check_file(vpath, pattern, size, off);
}

TEST_F(VolumeTest, uuid_clone)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const uint64_t vsize = 1 << 20;

    const auto volume_id(create_file(root_id,
                                     make_volume_name("volume").string(),
                                     vsize));

    const std::string pattern("Konstantin");
    const off_t off = 10 * get_cluster_size(volume_id);
    const uint64_t size = 30 * get_cluster_size(volume_id);

    write_to_file(volume_id, pattern, size, off);
    check_file(volume_id, pattern, size, off);

    check_stat(volume_id, vsize);
    client_.set_volume_as_template(volume_id);

    const vfs::FrontendPath clone_path(make_volume_name("/the_clone"));

    const vfs::ObjectId
        clone_id(fs_->create_clone(clone_path,
                                   make_metadata_backend_config(),
                                   vd::VolumeId(volume_id.str()),
                                   boost::none).str());
    verify_presence(clone_path);

    {
        struct stat st;
        memset(&st, 0x0, sizeof(st));

        ASSERT_EQ(0, getattr(clone_id, st));
        ASSERT_GT(st.st_size, 0);
    }

    const vfs::FrontendPath vpath(clone_path_to_volume_path(clone_path));
    const vfs::ObjectId vpath_id(*find_object(vpath));

    check_stat(vpath_id, vsize);
    check_file(vpath_id, pattern, size, off);
}

TEST_F(VolumeTest, no_unlinking_of_used_templates)
{
    const uint64_t vsize = 1 << 20;

    const vfs::FrontendPath templ_path(make_volume_name("/template"));
    const vfs::ObjectId templ_id(create_file(templ_path, vsize));
    const std::string pattern("Orpheus sat gloomy in his garden shed\n"
                              "Wondering what to do\n"
                              "With a lump of wood and a piece of wire\n"
                              "And a little pot of glue\n"
                              "Oh mama, oh mama");

    const off_t off = 13;

    write_to_file(templ_path, pattern, pattern.size(), off);
    check_file(templ_path, pattern, pattern.size(), off);

    client_.set_volume_as_template(templ_id);

    const vfs::FrontendPath clone_path(make_volume_name("/clone"));
    const vfs::ObjectId clone_id(fs_->create_clone(clone_path,
                                                   make_metadata_backend_config(),
                                                   vd::VolumeId(templ_id.str()),
                                                   boost::none));

    const vfs::FrontendPath clone_vpath(clone_path_to_volume_path(clone_path));

    verify_presence(clone_path);
    verify_presence(clone_vpath);

    EXPECT_NE(0, unlink(templ_path));
    verify_presence(templ_path);

    EXPECT_EQ(0, unlink(clone_vpath));
    verify_absence(clone_vpath);

    EXPECT_EQ(0, unlink(templ_path));
    verify_absence(templ_path);
}

TEST_F(VolumeTest, uuid_no_unlinking_of_used_templates)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const uint64_t vsize = 1 << 20;

    const vfs::FrontendPath templ_path(make_volume_name("/template"));
    const vfs::ObjectId templ_id(create_file(root_id,
                                             templ_path.str().substr(1),
                                             vsize));
    const std::string pattern("Orpheus sat gloomy in his garden shed\n"
                              "Wondering what to do\n"
                              "With a lump of wood and a piece of wire\n"
                              "And a little pot of glue\n"
                              "Oh mama, oh mama");

    const off_t off = 13;

    write_to_file(templ_id, pattern, pattern.size(), off);
    check_file(templ_id, pattern, pattern.size(), off);

    client_.set_volume_as_template(templ_id);

    const vfs::FrontendPath clone_path(make_volume_name("/clone"));
    const vfs::ObjectId clone_id(fs_->create_clone(clone_path,
                                                   make_metadata_backend_config(),
                                                   vd::VolumeId(templ_id.str()),
                                                   boost::none));

    const vfs::FrontendPath clone_vpath(clone_path_to_volume_path(clone_path));
    const vfs::ObjectId clone_vpath_id(*find_object(clone_vpath));

    verify_presence(clone_id);
    verify_presence(clone_vpath_id);

    EXPECT_NE(0, unlink(root_id,
                        templ_path.str().substr(1)));
    verify_presence(templ_id);

    EXPECT_EQ(0, unlink(root_id,
                        clone_vpath.str().substr(1)));
    verify_absence(clone_vpath_id);

    EXPECT_EQ(0, unlink(root_id,
                        templ_path.str().substr(1)));
    verify_absence(templ_id);
}

TEST_F(VolumeTest, clone_path_collisions)
{
    const vfs::FrontendPath parent_path(make_volume_name("/volume"));

    struct stat st;
    EXPECT_EQ(-ENOENT, getattr(parent_path, st));

    const uint64_t vsize = 1 << 20;

    const vfs::ObjectId parent_id(create_file(parent_path, vsize));
    client_.set_volume_as_template(parent_id);

    const vfs::FrontendPath clone_path(make_volume_name("/the_clone"));

    const vfs::ObjectId
        clone_id(fs_->create_clone(clone_path,
                                   make_metadata_backend_config(),
                                   vd::VolumeId(parent_id.str()),
                                   boost::none).str());
    verify_presence(clone_path);

    EXPECT_THROW(fs_->create_clone(clone_path,
                                   make_metadata_backend_config(),
                                   vd::VolumeId(parent_id.str()),
                                   boost::none),
                 std::exception);

    verify_presence(clone_path);
}

TEST_F(VolumeTest, uuid_clone_path_collisions)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath parent_path(make_volume_name("/volume"));

    struct stat st;
    EXPECT_EQ(-ENOENT, getattr(parent_path, st));

    const uint64_t vsize = 1 << 20;

    const vfs::ObjectId parent_id(create_file(root_id,
                                              parent_path.str().substr(1),
                                              vsize));
    client_.set_volume_as_template(parent_id);

    const vfs::FrontendPath clone_path(make_volume_name("/the_clone"));

    const vfs::ObjectId
        clone_id(fs_->create_clone(clone_path,
                                   make_metadata_backend_config(),
                                   vd::VolumeId(parent_id.str()),
                                   boost::none).str());
    verify_presence(clone_id);

    EXPECT_THROW(fs_->create_clone(clone_path,
                                   make_metadata_backend_config(),
                                   vd::VolumeId(parent_id.str()),
                                   boost::none),
                 std::exception);

    verify_presence(clone_id);
}

TEST_F(VolumeTest, clone_intermediate_dirs)
{
    const vfs::FrontendPath parent_path(make_volume_name("/volume"));

    const uint64_t vsize = 1 << 20;
    const vfs::ObjectId parent_id(create_file(parent_path, vsize));
    client_.set_volume_as_template(parent_id);

    const vfs::FrontendPath
        clone_path( std::string("/some/deep/non/existing/directory/structure/the_clone") +
                    fs_->vdisk_format().volume_suffix());

    const vfs::ObjectId
        clone_id(fs_->create_clone(clone_path,
                                   make_metadata_backend_config(),
                                   vd::VolumeId(parent_id.str()),
                                   boost::none).str());

    verify_presence(clone_path);

    const vfs::FrontendPath vpath(clone_path_to_volume_path(clone_path));
    check_stat(vpath, vsize);
}

TEST_F(VolumeTest, uuid_clone_intermediate_dirs)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath parent_path(make_volume_name("/volume"));

    const uint64_t vsize = 1 << 20;
    const vfs::ObjectId parent_id(create_file(root_id,
                                              parent_path.str().substr(1),
                                              vsize));
    client_.set_volume_as_template(parent_id);

    const vfs::FrontendPath
        clone_path( std::string("/some/deep/non/existing/directory/structure/the_clone") +
                    fs_->vdisk_format().volume_suffix());

    const vfs::ObjectId
        clone_id(fs_->create_clone(clone_path,
                                   make_metadata_backend_config(),
                                   vd::VolumeId(parent_id.str()),
                                   boost::none).str());

    verify_presence(clone_id);

    const vfs::FrontendPath vpath(clone_path_to_volume_path(clone_path));
    const vfs::ObjectId vpath_id(*find_object(vpath));
    check_stat(vpath_id, vsize);
}

TEST_F(VolumeTest, clone_file_exists)
{
    const vfs::FrontendPath parent_path(make_volume_name("/volume"));

    const uint64_t vsize = 1 << 20;
    const vfs::ObjectId parent_id(create_file(parent_path, vsize));
    client_.set_volume_as_template(parent_id);

    EXPECT_THROW(client_.create_clone_from_template(parent_path.str(),
                                                    make_metadata_backend_config(),
                                                    parent_id),
                 vfs::clienterrors::FileExistsException);

    const vfs::FrontendPath fpath(make_volume_name("/existing_file"));
    // Trick to create file with volumename without creating a volume, cause
    // we don't create volumes after rename
    {
        const vfs::FrontendPath tmp("/file");
        create_file(tmp);
        fs_->rename(tmp, fpath);
    }

    EXPECT_THROW(client_.create_clone_from_template(fpath.str(),
                                                    make_metadata_backend_config(),
                                                    parent_id),
                  vfs::clienterrors::FileExistsException);

    EXPECT_EQ(0, unlink(fpath));
    verify_absence(fpath);

    client_.create_clone_from_template(fpath.str(),
                                       make_metadata_backend_config(),
                                       parent_id);

    verify_presence(fpath);
}

TEST_F(VolumeTest, uuid_clone_file_exists)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath parent_path(make_volume_name("/volume"));

    const uint64_t vsize = 1 << 20;
    const vfs::ObjectId parent_id(create_file(root_id,
                                              parent_path.str().substr(1),
                                              vsize));
    client_.set_volume_as_template(parent_id);

    EXPECT_THROW(client_.create_clone_from_template(parent_path.str(),
                                                    make_metadata_backend_config(),
                                                    parent_id),
                 vfs::clienterrors::FileExistsException);

    const vfs::FrontendPath fpath(make_volume_name("/existing_file"));
    // Trick to create file with volumename without creating a volume, cause
    // we don't create volumes after rename
    {
        create_file(root_id,
                    "file");
        fs_->rename(root_id,
                    "file",
                    root_id,
                    fpath.str().substr(1));
    }

    EXPECT_THROW(client_.create_clone_from_template(fpath.str(),
                                                    make_metadata_backend_config(),
                                                    parent_id),
                  vfs::clienterrors::FileExistsException);

    const vfs::ObjectId fpath_id(*find_object(fpath));

    EXPECT_EQ(0, unlink(root_id,
                        fpath.str().substr(1)));
    verify_absence(fpath_id);

    client_.create_clone_from_template(fpath.str(),
                                       make_metadata_backend_config(),
                                       parent_id);

    verify_absence(fpath_id);
}

TEST_F(VolumeTest, chmod)
{
    const vfs::FrontendPath v(make_volume_name("/volume"));
    create_file(v);

    test_chmod_file(v, S_IWUSR bitor S_IRUSR bitor S_IRGRP bitor S_IROTH);
}

TEST_F(VolumeTest, uuid_chmod)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath v(make_volume_name("/volume"));

    create_file(root_id,
                v.str().substr(1));

    const vfs::ObjectId id(*find_object(v));

    test_chmod_file(id, S_IWUSR bitor S_IRUSR bitor S_IRGRP bitor S_IROTH);
}

TEST_F(VolumeTest, volume_entry_cache)
{
    const vfs::FrontendPath path(make_volume_name("/volume"));
    vfs::DirectoryEntryPtr entry(find_in_volume_entry_cache(path));
    ASSERT_TRUE(entry == nullptr);

    const vfs::ObjectId id(create_file(path));

    entry = find_in_volume_entry_cache(path);
    ASSERT_TRUE(entry != nullptr);
    EXPECT_EQ(id, entry->object_id());

    destroy_volume(path);

    EXPECT_FALSE(find_in_volume_entry_cache(path));
}

TEST_F(VolumeTest, uuid_volume_entry_cache)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath path(make_volume_name("/volume"));
    vfs::DirectoryEntryPtr entry(find_in_volume_entry_cache(path));
    ASSERT_TRUE(entry == nullptr);

    const vfs::ObjectId id(create_file(root_id,
                                       path.str().substr(1)));

    entry = find_in_volume_entry_cache(id);
    ASSERT_TRUE(entry != nullptr);
    EXPECT_EQ(id, entry->object_id());

    destroy_volume(root_id,
                   path.str().substr(1));

    EXPECT_FALSE(find_in_volume_entry_cache(id));
}

TEST_F(VolumeTest, rollback_volume)
{
    const vfs::FrontendPath volume_path(make_volume_name("/volume"));
    const uint64_t vsize = 1 << 20;
    const vfs::ObjectId vol_id(create_file(volume_path, vsize));
    const off_t off = 10 * get_cluster_size(vol_id);
    const uint64_t size = 30 * get_cluster_size(vol_id);

    const std::string patternA("Konstantin");
    write_to_file(volume_path, patternA, size, off);
    std::string snapA = client_.create_snapshot(vol_id);

    const std::string patternB("Erik");
    write_to_file(volume_path, patternB, size, off);

    wait_for_snapshot(vol_id, snapA);
    std::string snapB = client_.create_snapshot(vol_id);

    const std::string patternC("Simon");
    write_to_file(volume_path, patternC, size, off);

    check_snapshots(vol_id, {snapA, snapB});

    check_file(volume_path, patternC, size, off);

    wait_for_snapshot(vol_id, snapB);

    ASSERT_THROW(client_.rollback_volume(vol_id, "non-existing snapshot"),
                 vfs::clienterrors::SnapshotNotFoundException);

    client_.rollback_volume(vol_id, snapB);
    check_file(volume_path, patternB, size, off);
    write_to_file(volume_path, patternC, size, off);
    check_snapshots(vol_id, {snapA, snapB});

    client_.rollback_volume(vol_id, snapA);
    check_file(volume_path, patternA, size, off);
    check_snapshots(vol_id, {snapA});
}

TEST_F(VolumeTest, uuid_rollback_volume)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));

    const uint64_t vsize = 1 << 20;
    const vfs::ObjectId vol_id(create_file(root_id,
                                           make_volume_name("volume").string(),
                                           vsize));
    const off_t off = 10 * get_cluster_size(vol_id);
    const uint64_t size = 30 * get_cluster_size(vol_id);

    const std::string patternA("Konstantin");
    write_to_file(vol_id, patternA, size, off);
    std::string snapA = client_.create_snapshot(vol_id);

    const std::string patternB("Erik");
    write_to_file(vol_id, patternB, size, off);

    wait_for_snapshot(vol_id, snapA);
    std::string snapB = client_.create_snapshot(vol_id);

    const std::string patternC("Simon");
    write_to_file(vol_id, patternC, size, off);

    check_snapshots(vol_id, {snapA, snapB});

    check_file(vol_id, patternC, size, off);

    wait_for_snapshot(vol_id, snapB);

    ASSERT_THROW(client_.rollback_volume(vol_id, "non-existing snapshot"),
                 vfs::clienterrors::SnapshotNotFoundException);

    client_.rollback_volume(vol_id, snapB);
    check_file(vol_id, patternB, size, off);
    write_to_file(vol_id, patternC, size, off);
    check_snapshots(vol_id, {snapA, snapB});

    client_.rollback_volume(vol_id, snapA);
    check_file(vol_id, patternA, size, off);
    check_snapshots(vol_id, {snapA});
}

TEST_F(VolumeTest, unlink_unregistered_volume)
{
    const vfs::FrontendPath fname(make_volume_name("/volume"));
    const vfs::ObjectId id(create_file(fname));

    verify_presence(fname);

    fs_->object_router().object_registry()->unregister(id);

    EXPECT_EQ(0, unlink(fname));

    verify_absence(fname);

    EXPECT_EQ(-ENOENT, unlink(fname));
}

TEST_F(VolumeTest, uuid_unlink_unregistered_volume)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath fname(make_volume_name("volume"));
    const vfs::ObjectId id(create_file(root_id, fname.str()));

    verify_presence(id);

    fs_->object_router().object_registry()->unregister(id);

    EXPECT_EQ(0, unlink(root_id, fname.str()));

    verify_absence(id);

    EXPECT_EQ(-ENOENT, unlink(root_id, fname.str()));
}

TEST_F(VolumeTest, unlink_deleted_but_still_registered_volume)
{
    const vfs::FrontendPath fname(make_volume_name("/volume"));
    const vfs::ObjectId id(create_file(fname));

    verify_presence(fname);

    {
        LOCKVD();
        api::destroyVolume(vd::VolumeId(id.str()),
                           vd::DeleteLocalData::T,
                           vd::RemoveVolumeCompletely::T,
                           vd::DeleteVolumeNamespace::F,
                           vd::ForceVolumeDeletion::T);
    }

    EXPECT_EQ(0, unlink(fname));

    verify_absence(fname);

    EXPECT_EQ(-ENOENT, unlink(fname));
}

TEST_F(VolumeTest, uuid_unlink_deleted_but_still_registered_volume)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath fname(make_volume_name("volume"));
    const vfs::ObjectId id(create_file(root_id, fname.str()));

    verify_presence(id);

    {
        LOCKVD();
        api::destroyVolume(vd::VolumeId(id.str()),
                           vd::DeleteLocalData::T,
                           vd::RemoveVolumeCompletely::T,
                           vd::DeleteVolumeNamespace::F,
                           vd::ForceVolumeDeletion::T);
    }

    EXPECT_EQ(0, unlink(root_id, fname.str()));

    verify_absence(id);

    EXPECT_EQ(-ENOENT, unlink(root_id, fname.str()));
}

TEST_F(VolumeTest, chown)
{
    const vfs::FrontendPath fname(make_volume_name("/volume"));
    create_file(fname);

    test_chown(fname);
}

TEST_F(VolumeTest, uuid_chown)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const std::string fname(make_volume_name("volume").string());
    const vfs::ObjectId id(create_file(root_id, fname));

    test_chown(id);
}

TEST_F(VolumeTest, start_while_running)
{
    const vfs::FrontendPath fname(make_volume_name("/volume"));
    const uint64_t vsize = 1 << 20;
    const vfs::ObjectId id(create_file(fname, vsize));

    const std::string pattern("some data");
    const uint64_t wsize = pattern.size();

    EXPECT_EQ(static_cast<ssize_t>(wsize),
              write_to_file(fname, pattern, wsize, 0));
    check_file(fname, pattern, wsize, 0);

    fs_->object_router().migrate(id);

    check_file(fname, pattern, wsize, 0);
}

TEST_F(VolumeTest, uuid_start_while_running)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const std::string fname(make_volume_name("volume").string());
    const uint64_t vsize = 1 << 20;
    const vfs::ObjectId id(create_file(root_id,
                                       fname,
                                       vsize));

    const std::string pattern("some data");
    const uint64_t wsize = pattern.size();

    EXPECT_EQ(static_cast<ssize_t>(wsize),
              write_to_file(id, pattern, wsize, 0));
    check_file(id, pattern, wsize, 0);

    fs_->object_router().migrate(id);

    check_file(id, pattern, wsize, 0);
}

TEST_F(VolumeTest, stop_and_start)
{
    const vfs::FrontendPath fname(make_volume_name("/volume"));
    const uint64_t vsize = 1 << 20;
    const vfs::ObjectId id(create_file(fname, vsize));

    const std::string pattern1("written-first");
    const uint64_t wsize = pattern1.size();

    EXPECT_EQ(static_cast<ssize_t>(wsize),
              write_to_file(fname, pattern1, wsize, 0));
    check_file(fname, pattern1, wsize, 0);

    fs_->object_router().stop(id);

    {
        const std::string pattern2("written-while-stopped");

        EXPECT_GT(0, write_to_file(fname, pattern2, pattern2.size(), 0));

        std::vector<char> rbuf(wsize);
        EXPECT_GT(0, read_from_file(fname, rbuf.data(), rbuf.size(), 0));
    }

    fs_->object_router().migrate(id);

    check_file(fname, pattern1, wsize, 0);
}

TEST_F(VolumeTest, uuid_stop_and_start)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const std::string fname(make_volume_name("volume").string());
    const uint64_t vsize = 1 << 20;
    const vfs::ObjectId id(create_file(root_id,
                                       fname,
                                       vsize));

    const std::string pattern1("written-first");
    const uint64_t wsize = pattern1.size();

    EXPECT_EQ(static_cast<ssize_t>(wsize),
              write_to_file(id, pattern1, wsize, 0));
    check_file(id, pattern1, wsize, 0);

    fs_->object_router().stop(id);

    {
        const std::string pattern2("written-while-stopped");

        EXPECT_GT(0, write_to_file(id, pattern2, pattern2.size(), 0));

        std::vector<char> rbuf(wsize);
        EXPECT_GT(0, read_from_file(id, rbuf.data(), rbuf.size(), 0));
    }

    fs_->object_router().migrate(id);

    check_file(id, pattern1, wsize, 0);
}

TEST_F(VolumeTest, hygiene)
{
    const vfs::FrontendPath fname(make_volume_name("/volume"));
    const uint64_t vsize = 1 << 20;
    const vfs::ObjectId id(create_file(fname, vsize));

    const std::string pattern1("not of any importance");
    const uint64_t wsize = pattern1.size();

    EXPECT_EQ(static_cast<ssize_t>(wsize),
              write_to_file(fname, pattern1, wsize, 0));
    check_file(fname, pattern1, wsize, 0);

    auto conn(cm_->getConnection());
    vd::VolumeConfig cfg;

    {
        LOCKVD();
        cfg = api::getVolumeConfig(vd::VolumeId(id.str()));
    }

    EXPECT_TRUE(conn->namespaceExists(cfg.getNS()));

    unlink(fname);
    verify_absence(fname);

    LOCKVD();
    EXPECT_FALSE(conn->namespaceExists(cfg.getNS()));
}

TEST_F(VolumeTest, uuid_hygiene)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const std::string fname(make_volume_name("volume").string());
    const uint64_t vsize = 1 << 20;
    const vfs::ObjectId id(create_file(root_id,
                                       fname,
                                       vsize));

    const std::string pattern1("not of any importance");
    const uint64_t wsize = pattern1.size();

    EXPECT_EQ(static_cast<ssize_t>(wsize),
              write_to_file(id, pattern1, wsize, 0));
    check_file(id, pattern1, wsize, 0);

    auto conn(cm_->getConnection());
    vd::VolumeConfig cfg;

    {
        LOCKVD();
        cfg = api::getVolumeConfig(vd::VolumeId(id.str()));
    }

    EXPECT_TRUE(conn->namespaceExists(cfg.getNS()));

    unlink(root_id,
           fname);
    verify_absence(id);

    LOCKVD();
    EXPECT_FALSE(conn->namespaceExists(cfg.getNS()));
}

TEST_F(VolumeTest, update_mds_config)
{
    const auto mdb_type = mdstore_test_setup_->backend_type_;
    if (mdb_type != vd::MetaDataBackendType::MDS)
    {
        LOG_INFO("This test does not do anything with a " << mdb_type <<
                 " metadata backend. Re-run this test with an MDS metadata backend");
        return;
    }

    bpt::ptree pt;
    fs_->persist(pt,
                 ReportDefault::F);

    const ip::PARAMETER_TYPE(fs_metadata_backend_mds_nodes) old_mds_nodes(pt);

    const vfs::FrontendPath vname1(make_volume_name("/volume1"));
    const vfs::ObjectId id1(create_file(vname1));

    auto check_mds_config([&](const vfs::ObjectId& oid,
                              const vd::MDSNodeConfigs& ncfgs)
    {
        LOCKVD();
        const vd::VolumeConfig& cfg =
            api::getVolumeConfig(vd::VolumeId(oid.str()));

        ASSERT_EQ(mdb_type,
                  cfg.metadata_backend_config_->backend_type());

        const auto& mds_cfg(dynamic_cast<const vd::MDSMetaDataBackendConfig&>(*cfg.metadata_backend_config_));

        ASSERT_EQ(ncfgs,
                  mds_cfg.node_configs());
    });

    check_mds_config(id1,
                     old_mds_nodes.value());

    const mds::ServerConfig scfg(mds_test_setup_->next_server_config());
    mds_manager_->start_one(scfg);

    const ip::PARAMETER_TYPE(fs_metadata_backend_mds_nodes)
        new_mds_nodes(vd::MDSNodeConfigs{ scfg.node_config });

    ASSERT_FALSE(old_mds_nodes.value().empty());
    ASSERT_FALSE(new_mds_nodes.value().empty());

    ASSERT_NE(old_mds_nodes.value(),
              new_mds_nodes.value());

    new_mds_nodes.persist(pt);

    yt::UpdateReport urep;
    fs_->update(pt,
                urep);

    ASSERT_EQ(1U, urep.update_size());

    const vfs::FrontendPath vname2(make_volume_name("/volume2"));
    const vfs::ObjectId id2(create_file(vname2));

    check_mds_config(id2,
                     new_mds_nodes.value());
}

TEST_F(VolumeTest, owner_tag)
{
    const vfs::FrontendPath fname(make_volume_name("/volume"));
    const vfs::ObjectId oid(create_file(fname));
    const vfs::ObjectRegistrationPtr reg(find_registration(oid));

    LOCKVD();

    const vd::VolumeConfig cfg(api::getVolumeConfig(static_cast<vd::VolumeId>(oid)));
    EXPECT_EQ(reg->owner_tag,
              cfg.owner_tag_);
}

}

// Local Variables: **
// mode: c++ **
// End: **
