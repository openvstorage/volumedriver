// Copyright 2015 Open vStorage NV
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

#include <boost/foreach.hpp>

#include "FuseFSTestSetup.h"

namespace fawltyfstest
{

namespace fs = boost::filesystem;

class FuseFSTest
    : public FuseFSTestSetup
{
public:
    FuseFSTest()
        : FuseFSTestSetup("FuseFSTest")
    {}
};

TEST_F(FuseFSTest, fileBasics)
{
    const fs::path file("/testfile");
    struct fuse_file_info fi;

    bzero(&fi, sizeof(fi));
    fi.flags = O_RDWR;

    ASSERT_GT(0, fuse_fs_open(fuse_fs_,
                               file.string().c_str(),
                               &fi));

    // ASSERT_FALSE(fs::exists(path_ / file));

    // dev_t devt = makedev(0, 0);
    // ASSERT_EQ(0, fuse_fs_mknod(fuse_fs_,
    //                            file.string().c_str(),
    //                            S_IFREG,
    //                            devt));
    // ASSERT_TRUE(fs::exists(path_ / file));

    // ASSERT_GT(0, fuse_fs_open(fuse_fs_,
    //                           file.string().c_str(),
    //                           &fi));

    // ASSERT_EQ(0, fuse_fs_chmod(fuse_fs_,
    //                            file.string().c_str(),
    //                            S_IWUSR|S_IRUSR));

    // ASSERT_EQ(0, fuse_fs_open(fuse_fs_,
    //                           file.string().c_str(),
    //                           &fi));

    // const std::vector<char> wbuf(16 << 10, 'X');
    // ASSERT_EQ(static_cast<int>(wbuf.size()), fuse_fs_write(fuse_fs_,
    //                                                        file.string().c_str(),
    //                                                        &wbuf[0],
    //                                                        wbuf.size(),
    //                                                        0,
    //                                                        &fi));

    // ASSERT_EQ(0, fuse_fs_fsync(fuse_fs_,
    //                            file.string().c_str(),
    //                            1,
    //                            &fi));

    // ASSERT_EQ(0, fuse_fs_fsync(fuse_fs_,
    //                            file.string().c_str(),
    //                            0,
    //                            &fi));

    // std::vector<char> rbuf(32 << 10);

    // ASSERT_EQ(static_cast<int>(wbuf.size()), fuse_fs_read(fuse_fs_,
    //                                                       file.string().c_str(),
    //                                                       &rbuf[0],
    //                                                       wbuf.size(),
    //                                                       0,
    //                                                       &fi));

    // for (size_t i = 0; i < wbuf.size(); ++i)
    // {
    //     ASSERT_EQ(wbuf[i], rbuf[i]);
    // }

    // bzero(&rbuf[0], rbuf.size());

    // // short read
    // ASSERT_EQ(static_cast<int>(wbuf.size()), fuse_fs_read(fuse_fs_,
    //                                                       file.string().c_str(),
    //                                                       &rbuf[0],
    //                                                       rbuf.size(),
    //                                                       0,
    //                                                       &fi));

    // for (size_t i = 0; i < wbuf.size(); ++i)
    // {
    //     ASSERT_EQ(wbuf[i], rbuf[i]);
    // }

    // ASSERT_EQ(0, fuse_fs_release(fuse_fs_,
    //                              file.string().c_str(),
    //                              &fi));

    // struct stat st1;
    // bzero(&st1, sizeof(st1));

    // ASSERT_EQ(0, fuse_fs_getattr(fuse_fs_,
    //                              file.string().c_str(),
    //                              &st1));

    // struct stat st2;
    // bzero(&st2, sizeof(st2));

    // ASSERT_EQ(0, stat((path_ / file).string().c_str(),
    //                   &st2));

    // ASSERT_EQ(0, memcmp(&st1, &st2, sizeof(struct stat)));

    // ASSERT_EQ(0, fuse_fs_unlink(fuse_fs_,
    //                             file.string().c_str()));

    // ASSERT_FALSE(fs::exists(path_ / file));
}

// namespace
// {

// struct dirinfo
// {
//     dirinfo(const std::string& iname,
//             const struct stat& ist,
//             off_t ioff)
//         : name(iname)
//         , stat(ist)
//         , off(ioff)
//     {}

//     const std::string name;
//     const struct stat stat;
//     const off_t off;
// };

// typedef std::list<dirinfo> dirinfo_list;

// int testfiller(void* data,
//                const char* name,
//                const struct stat* stbuf,
//                off_t off)
// {
//     dirinfo_list* l = static_cast<dirinfo_list*>(data);
//     l->push_back(dirinfo(name, *stbuf, off));
//     return 0;
// }
// }

// TEST_F(FuseFSTest, directoryBasics)
// {
//     const fs::path dir("/testdirectory");
//     struct fuse_file_info fi;

//     bzero(&fi, sizeof(fi));

//     ASSERT_GT(0, fuse_fs_opendir(fuse_fs_,
//                                  dir.string().c_str(),
//                                  &fi));

//     ASSERT_FALSE(fs::exists(path_ / dir));

//     ASSERT_EQ(0, fuse_fs_mkdir(fuse_fs_,
//                                dir.string().c_str(),
//                                S_IFDIR|S_IRWXU));

//     ASSERT_TRUE(fs::exists(path_ / dir));

//     {
//         ASSERT_EQ(0, fuse_fs_opendir(fuse_fs_,
//                                      dir.string().c_str(),
//                                      &fi));

//         dirinfo_list l;
//         ASSERT_EQ(0, fuse_fs_readdir(fuse_fs_,
//                                      dir.string().c_str(),
//                                      &l,
//                                      testfiller,
//                                      0,
//                                      &fi));

//         ASSERT_EQ(static_cast<size_t>(2), l.size());
//         BOOST_FOREACH(dirinfo& i, l)
//         {
//             ASSERT_TRUE(i.name == "." or
//                         i.name == "..");
//         }

//         ASSERT_EQ(0, fuse_fs_releasedir(fuse_fs_,
//                                         dir.string().c_str(),
//                                         &fi));
//     }

//     const fs::path subdir(dir / "subdir");

//     ASSERT_EQ(0, fuse_fs_mkdir(fuse_fs_,
//                                subdir.string().c_str(),
//                                S_IFDIR|S_IRWXU));

//     {
//         ASSERT_EQ(0, fuse_fs_opendir(fuse_fs_,
//                                      dir.string().c_str(),
//                                      &fi));

//         dirinfo_list l;
//         ASSERT_EQ(0, fuse_fs_readdir(fuse_fs_,
//                                      dir.string().c_str(),
//                                      &l,
//                                      testfiller,
//                                      0,
//                                      &fi));

//         ASSERT_EQ(static_cast<size_t>(3), l.size());

//         BOOST_FOREACH(dirinfo& i, l)
//         {
//             ASSERT_TRUE(i.name == "." or
//                         i.name == ".." or
//                         i.name == subdir.leaf());
//         }

//         ASSERT_EQ(0, fuse_fs_releasedir(fuse_fs_,
//                                         dir.string().c_str(),
//                                         &fi));

//     }

//     ASSERT_EQ(0, fuse_fs_rmdir(fuse_fs_,
//                                subdir.string().c_str()));

//     ASSERT_EQ(0, fuse_fs_rmdir(fuse_fs_,
//                                dir.string().c_str()));

//     ASSERT_FALSE(fs::exists(path_ / dir));
// }

// TEST_F(FuseFSTest, DISABLED_unlinkOpenFileAndRemoveDirectory)
// {
//     const fs::path dir("testdir");
//     const fs::path file(dir / "testfile");

//     ASSERT_EQ(0, fuse_fs_mkdir(fuse_fs_,
//                                dir.string().c_str(),
//                                S_IFDIR|S_IRWXU));

//     ASSERT_TRUE(fs::exists(path_ / dir));

//     struct fuse_file_info fi;
//     bzero(&fi, sizeof(fi));

//     fi.flags = O_RDWR|O_CREAT;

//     ASSERT_EQ(0, fuse_fs_open(fuse_fs_,
//                               file.string().c_str(),
//                               &fi));

//     ASSERT_TRUE(fs::exists(path_ / file));

//     ASSERT_EQ(0, fuse_fs_unlink(fuse_fs_,
//                                 file.string().c_str()));

//     const std::vector<char> wbuf(4096, 'X');
//     ASSERT_EQ(static_cast<int>(wbuf.size()), fuse_fs_write(fuse_fs_,
//                                                            file.string().c_str(),
//                                                            &wbuf[0],
//                                                            wbuf.size(),
//                                                            0,
//                                                            &fi));

//     std::vector<char> rbuf(wbuf.size(), 'A');
//     ASSERT_EQ(static_cast<int>(rbuf.size()), fuse_fs_read(fuse_fs_,
//                                                           file.string().c_str(),
//                                                           &rbuf[0],
//                                                           rbuf.size(),
//                                                           0,
//                                                           &fi));

//     ASSERT_FALSE(fs::exists(path_ / file));

//     ASSERT_EQ(0, fuse_fs_rmdir(fuse_fs_,
//                                dir.string().c_str()));

//     ASSERT_FALSE(fs::exists(path_ / dir));

//     ASSERT_EQ(0, fuse_fs_release(fuse_fs_,
//                                  file.string().c_str(),
//                                  &fi));
// }

}

// Local Variables: **
// mode: c++ **
// End: **
