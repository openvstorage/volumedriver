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

#include <future>
#include <initializer_list>

#include <simple-nfs/File.h>
#include <simple-nfs/DirectoryEntry.h>
#include <simple-nfs/Mount.h>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/discrete_distribution.hpp>
#include <boost/property_tree/string_path.hpp>

#include <youtils/SignalSet.h>
#include <youtils/SignalThread.h>

namespace ganeshatest
{

namespace fs = boost::filesystem;
namespace vd = volumedriver;
namespace yt = youtils;

class SimpleTest
    : public Setup
{
protected:
    SimpleTest()
        : Setup(volumedriverfstest::FileSystemTestSetupParameters("SimpleGaneshaTest"))
    {}

    void
    SetUp()
    {
        Setup::SetUp();
        ASSERT_NO_THROW(nfs_.reset(new simple_nfs::Mount(nfs_server(),
                                                         nfs_export())));
    }

    void
    TearDown()
    {
        if (nfs_)
        {
            nfs_.reset();
        }

        Setup::TearDown();
    }

    void
    create_file(const fs::path& fname)
    {
        nfs_->open(fname, yt::FDMode::ReadWrite, CreateIfNecessary::T);
    }

    DECLARE_LOGGER("SimpleGaneshaTest");

    const fs::path root_ = { "/" };
    std::unique_ptr<simple_nfs::Mount> nfs_;
};

TEST_F(SimpleTest, mount)
{}

TEST_F(SimpleTest, list_empty_directory)
{
    ASSERT_TRUE(nfs_->readdir(root_).empty());
}

TEST_F(SimpleTest, create_and_remove_file)
{
    ASSERT_TRUE(nfs_->readdir(root_).empty());

    const fs::path testfile("/testfile");
    create_file(testfile);

    const std::list<simple_nfs::DirectoryEntry> l(nfs_->readdir(root_));

    ASSERT_EQ(1U, l.size());
    simple_nfs::DirectoryEntry de = l.front();

    ASSERT_EQ(testfile.filename(), de.name);
    ASSERT_TRUE(simple_nfs::EntryType::NF3REG == de.type);

    EXPECT_FALSE(client_.get_volume_id(testfile.string()));

    const struct stat st(nfs_->stat(testfile));
    EXPECT_TRUE(S_ISREG(st.st_mode));
    EXPECT_EQ(0U, st.st_size);

    nfs_->unlink(testfile);
    ASSERT_TRUE(nfs_->readdir(root_).empty());
}

TEST_F(SimpleTest, create_and_remove_dir)
{
    ASSERT_TRUE(nfs_->readdir(root_).empty());

    const fs::path testdir("/testdir");
    nfs_->mkdir(testdir);

    {
        const std::list<simple_nfs::DirectoryEntry> l(nfs_->readdir(root_));
        ASSERT_EQ(1U, l.size());
        simple_nfs::DirectoryEntry de = l.front();
        ASSERT_EQ(testdir.filename(), de.name);
        ASSERT_TRUE(simple_nfs::EntryType::NF3DIR == de.type);
    }

    ASSERT_TRUE(nfs_->readdir(testdir).empty());


    nfs_->rmdir(testdir);
    ASSERT_TRUE(nfs_->readdir(root_).empty());
}

TEST_F(SimpleTest, create_file_in_new_dir)
{
    ASSERT_TRUE(nfs_->readdir(root_).empty());

    const fs::path testdir("/testdir");
    nfs_->mkdir(testdir);

    {
        const std::list<simple_nfs::DirectoryEntry> l(nfs_->readdir(root_));
        ASSERT_EQ(1U, l.size());
        ASSERT_EQ(testdir.filename(), l.front().name);
        ASSERT_TRUE(simple_nfs::EntryType::NF3DIR == l.front().type);

        ASSERT_TRUE(nfs_->readdir(testdir).empty());
    }

    const fs::path testfile(testdir / "testfile");
    create_file(testfile);

    {
        const std::list<simple_nfs::DirectoryEntry> l(nfs_->readdir(testdir));
        ASSERT_EQ(1U, l.size());
        ASSERT_EQ(testfile.filename(), l.front().name);
        ASSERT_TRUE(simple_nfs::EntryType::NF3REG == l.front().type);
    }

    nfs_->unlink(testfile);
    nfs_->rmdir(testdir);
}

TEST_F(SimpleTest, create_subdirs)
{
    ASSERT_TRUE(nfs_->readdir(root_).empty());

    const fs::path dir1(root_ / "dir1");
    nfs_->mkdir(dir1);

    {
        const std::list<simple_nfs::DirectoryEntry> l(nfs_->readdir(root_));
        ASSERT_EQ(1U, l.size());
        ASSERT_EQ(dir1.filename(), l.front().name);
        ASSERT_TRUE(simple_nfs::EntryType::NF3DIR == l.front().type);
        ASSERT_TRUE(nfs_->readdir(dir1).empty());
    }

    const fs::path dir2(dir1 / "dir2");
    nfs_->mkdir(dir2);

    {
        const std::list<simple_nfs::DirectoryEntry> l(nfs_->readdir(dir1));
        ASSERT_EQ(1U, l.size());
        ASSERT_EQ(dir2.filename(), l.front().name);
        ASSERT_TRUE(simple_nfs::EntryType::NF3DIR == l.front().type);
        ASSERT_TRUE(nfs_->readdir(dir2).empty());
    }

    nfs_->rmdir(dir2);
    nfs_->rmdir(dir1);
}

TEST_F(SimpleTest, remove_non_empty_dir)
{
    ASSERT_TRUE(nfs_->readdir(root_).empty());

    const fs::path dir1(root_ / "dir1");
    nfs_->mkdir(dir1);

    {
        const std::list<simple_nfs::DirectoryEntry> l(nfs_->readdir(root_));
        ASSERT_EQ(1U, l.size());
        ASSERT_EQ(dir1.filename(), l.front().name);
        ASSERT_TRUE(simple_nfs::EntryType::NF3DIR == l.front().type);
        ASSERT_TRUE(nfs_->readdir(dir1).empty());
    }

    const fs::path file1(dir1 / "file1");
    create_file(file1);
    EXPECT_THROW(nfs_->rmdir(dir1),
                 simple_nfs::Exception);

    EXPECT_FALSE(nfs_->readdir(root_).empty());

    EXPECT_NO_THROW(nfs_->unlink(file1));

    ASSERT_NO_THROW(nfs_->rmdir(dir1));

    EXPECT_TRUE(nfs_->readdir(root_).empty());
}

TEST_F(SimpleTest, create_read_write_remove_file)
{
    ASSERT_TRUE(nfs_->readdir(root_).empty());
    const fs::path testfile("/testfile");
    simple_nfs::File testf(*nfs_,
                           testfile,
                           youtils::FDMode::ReadWrite,
                           CreateIfNecessary::T);
    {

        const std::list<simple_nfs::DirectoryEntry> l(nfs_->readdir(root_));

        ASSERT_EQ(1U, l.size());
        simple_nfs::DirectoryEntry de = l.front();

        ASSERT_EQ(testfile.filename(), de.name);
        ASSERT_TRUE(simple_nfs::EntryType::NF3REG == de.type);
    }
    const size_t buf_size = 4096;

    std::vector<char> buf(buf_size);

    int res = 0;

    EXPECT_NO_THROW(res = testf.pread(buf.data(),
                                      buf_size,
                                      0));
    EXPECT_EQ(0, res);

    std::vector<char> buf2(buf_size,
                           'a');

    ASSERT_NO_THROW(res = testf.pwrite(buf2.data(),
                                       buf_size,
                                       0));
    ASSERT_EQ(res, static_cast<ssize_t>(buf_size));

    ASSERT_NO_THROW(res = testf.pread(buf.data(),
                                      buf_size,
                                      0));
    EXPECT_TRUE(res == buf_size);

    ASSERT_TRUE(buf == buf2);

    nfs_->unlink(testfile);

    ASSERT_TRUE(nfs_->readdir(root_).empty());
}

TEST_F(SimpleTest, create_read_write_remove_files_and_dirs)
{
    ASSERT_TRUE(nfs_->readdir(root_).empty());
    const fs::path testfile1("/testfile1");
    simple_nfs::File testf1(*nfs_,
                           testfile1,
                           youtils::FDMode::ReadWrite,
                           CreateIfNecessary::T);

    const fs::path testdir("/testdir");
    ASSERT_NO_THROW(nfs_->mkdir(testdir));
    const fs::path testsubdir("/testdir/testsubdir");
    ASSERT_NO_THROW(nfs_->mkdir(testsubdir));
    const fs::path testfile2 = testsubdir / "testfile2";

    simple_nfs::File testf2(*nfs_,
                            testfile2,
                            youtils::FDMode::ReadWrite,
                            CreateIfNecessary::T);

    const size_t buf_size = 4096;
    std::vector<char> buf(buf_size);

    std::vector<char> buf1(buf_size, '1');
    std::vector<char> buf2(buf_size, '2');

    int res = 0;

    EXPECT_NO_THROW(res = testf1.pread(buf.data(),
                                       buf_size,
                                       0));
    EXPECT_EQ(res, 0);


    EXPECT_NO_THROW(res = testf1.pread(buf.data(),
                                       buf_size,
                                       0));
    EXPECT_EQ(res, 0);

    const size_t truncate_size = 1024;

    EXPECT_NO_THROW(testf1.truncate(0));
    EXPECT_NO_THROW(testf1.truncate(truncate_size));
    EXPECT_NO_THROW(res = testf1.pread(buf.data(),
                                       buf_size,
                                       0));
    EXPECT_EQ(res, static_cast<ssize_t>(truncate_size));

    EXPECT_NO_THROW(testf1.truncate(0));

    EXPECT_NO_THROW(res = testf1.pread(buf.data(),
                                       buf_size,
                                       0));
    EXPECT_EQ(res, 0);

    ASSERT_NO_THROW(res = testf1.pwrite(buf1.data(),
                                        buf_size,
                                        0));
    ASSERT_EQ(res, static_cast<ssize_t>(buf_size));

    ASSERT_NO_THROW(res = testf1.pread(buf.data(),
                                       buf_size,
                                       0));
}

TEST_F(SimpleTest, create_and_remove_volume)
{
    const fs::path vpath(make_volume_name("/testvolume"));

    simple_nfs::File vol(nfs_->open(vpath,
                                    yt::FDMode::ReadWrite,
                                    CreateIfNecessary::T));

    {

        const std::list<simple_nfs::DirectoryEntry> entries(nfs_->readdir(root_));
        ASSERT_EQ(1U, entries.size());

        const simple_nfs::DirectoryEntry& entry = entries.front();
        ASSERT_EQ(vpath.filename(), entry.name);
        ASSERT_EQ(0U, entry.size);

        ASSERT_TRUE(simple_nfs::EntryType::NF3REG == entry.type);

        const struct stat st(nfs_->stat(vpath));
        EXPECT_TRUE(S_ISREG(st.st_mode));
        EXPECT_EQ(0, st.st_size);
    }

    const uint64_t vsize = 1ULL << 20;
    vol.truncate(vsize);

    const std::list<simple_nfs::DirectoryEntry> entries(nfs_->readdir(root_));
    ASSERT_EQ(1U, entries.size());

    const simple_nfs::DirectoryEntry& entry = entries.front();
    ASSERT_EQ(vpath.filename(), entry.name);
    ASSERT_EQ(vsize, entry.size);

    ASSERT_TRUE(simple_nfs::EntryType::NF3REG == entry.type);

    ASSERT_TRUE(static_cast<bool>(client_.get_volume_id(vpath.string()))) << vpath << " is not a volume!?";

    const struct stat st(nfs_->stat(vpath));
    EXPECT_TRUE(S_ISREG(st.st_mode));
    EXPECT_EQ(static_cast<ssize_t>(vsize), st.st_size);

    nfs_->unlink(vpath);

    ASSERT_TRUE(nfs_->readdir(root_).empty());
}

TEST_F(SimpleTest, create_volume_clone_template)
{
    const volumedriverfs::FrontendPath vpath("/vsa001/volume.vmdk");
    const youtils::DimensionedValue size("1GiB");

    const volumedriverfs::ObjectId tname(client_.create_volume(vpath.str(),
                volumedriverfstest::FileSystemTestSetup::make_metadata_backend_config(),
                size));

    const struct stat st(nfs_->stat(vpath));
    EXPECT_TRUE(S_ISREG(st.st_mode));

    const volumedriverfs::FrontendPath cpath("/test-1/clone.vmdk");
    ASSERT_THROW(client_.create_clone_from_template(cpath.str(),
                volumedriverfstest::FileSystemTestSetup::make_metadata_backend_config(),
                tname.str()),
            volumedriverfs::clienterrors::InvalidOperationException);

    client_.set_volume_as_template(tname.str());
    const volumedriverfs::ObjectId cname(client_.create_clone_from_template(cpath.str(),
                volumedriverfstest::FileSystemTestSetup::make_metadata_backend_config(),
                tname.str()));

    const struct stat stc(nfs_->stat(cpath));
    EXPECT_TRUE(S_ISREG(stc.st_mode));
}

TEST_F(SimpleTest, write_and_read_volume)
{
    const fs::path vpath(make_volume_name("/testvolume"));

    simple_nfs::File vol(nfs_->open(vpath,
                                    yt::FDMode::ReadWrite,
                                    CreateIfNecessary::T));

    const uint64_t vsize = 1ULL << 20;
    vol.truncate(vsize);

    ASSERT_TRUE(static_cast<bool>(client_.get_volume_id(vpath.string()))) << vpath << " is not a volume!?";

    const std::vector<char> wbuf(4096, 'z');

    ASSERT_EQ(0U, vsize % wbuf.size());

    for (uint64_t off = 0; off < vsize; off += wbuf.size())
    {
        EXPECT_EQ(wbuf.size(), vol.pwrite(wbuf.data(), wbuf.size(), off));
    }

    for (int64_t off = vsize - wbuf.size(); off >= 0; off -= wbuf.size())
    {
        std::vector<char> rbuf(wbuf.size());
        EXPECT_EQ(rbuf.size(), vol.pread(rbuf.data(), rbuf.size(), off));
    }
}

TEST_F(SimpleTest, rename)
{
    const fs::path oldname("/file.old.name");
    create_file(oldname);

    const std::string pattern("old");

    {
        const struct stat st(nfs_->stat(oldname));
        EXPECT_TRUE(S_ISREG(st.st_mode));

        simple_nfs::File f(*nfs_,
                           oldname,
                           youtils::FDMode::Write,
                           CreateIfNecessary::F);

        f.pwrite(pattern.data(), pattern.size(), 0);
    }

    const fs::path newname("/file.new.name");

    EXPECT_THROW(nfs_->stat(newname),
                 simple_nfs::Exception);

    nfs_->rename(oldname, newname);

    EXPECT_THROW(nfs_->stat(oldname),
                 simple_nfs::Exception);

    {
        const struct stat st(nfs_->stat(newname));
        EXPECT_TRUE(S_ISREG(st.st_mode));

        simple_nfs::File f(*nfs_,
                           newname,
                           youtils::FDMode::Read,
                           CreateIfNecessary::F);

        std::vector<char> rbuf(pattern.size());
        f.pread(rbuf.data(), rbuf.size(), 0);
        std::string s(rbuf.data(), rbuf.size());

        EXPECT_EQ(pattern, s);
    }
}

TEST_F(SimpleTest, rename_to_same_target)
{
    const fs::path testdir("/testdir");

    nfs_->mkdir(testdir);

    const fs::path srcname(testdir / "file.name");
    create_file(srcname);

    const std::string pattern("file");

    {
        const struct stat st(nfs_->stat(srcname));
        EXPECT_TRUE(S_ISREG(st.st_mode));

        simple_nfs::File f(*nfs_,
                           srcname,
                           youtils::FDMode::Write,
                           CreateIfNecessary::F);

        f.pwrite(pattern.data(), pattern.size(), 0);
    }

    EXPECT_NO_THROW(nfs_->rename(srcname, srcname));

    {
        const struct stat st(nfs_->stat(srcname));
        EXPECT_TRUE(S_ISREG(st.st_mode));

        simple_nfs::File f(*nfs_,
                           srcname,
                           youtils::FDMode::Read,
                           CreateIfNecessary::F);

        std::vector<char> rbuf(pattern.size());
        f.pread(rbuf.data(), rbuf.size(), 0);
        std::string s(rbuf.data(), rbuf.size());

        EXPECT_EQ(pattern, s);
    }
}

TEST_F(SimpleTest, utimes)
{
    const fs::path testdir("/testdir");

    nfs_->mkdir(testdir);

    const fs::path testfile(testdir / "testfile");
    create_file(testfile);

    {
        const struct stat st(nfs_->stat(testfile));
        EXPECT_TRUE(S_ISREG(st.st_mode));

        struct timeval times[2];
        /* atime */
        times[0].tv_sec = 88;
        times[0].tv_usec = 0;
        /* mtime */
        times[1].tv_sec = 90;
        times[1].tv_usec = 0;
        nfs_->utimes(testfile, times);

        const struct stat ust(nfs_->stat(testfile));
        EXPECT_EQ(88, ust.st_atime);
        EXPECT_EQ(90, ust.st_mtime);
    }
}

TEST_F(SimpleTest, rename_to_existing_target)
{
    const fs::path testdir("/testdir");

    nfs_->mkdir(testdir);

    const fs::path oldname(testdir / "file.old.name");
    create_file(oldname);

    const fs::path newname(testdir / "file.new.name");
    create_file(newname);

    const std::string oldpattern("old");

    {
        const struct stat st(nfs_->stat(oldname));
        EXPECT_TRUE(S_ISREG(st.st_mode));

        simple_nfs::File f(*nfs_,
                           oldname,
                           youtils::FDMode::Write,
                           CreateIfNecessary::F);

        f.pwrite(oldpattern.data(), oldpattern.size(), 0);
    }

    const std::string newpattern("new");

    {
        const struct stat st(nfs_->stat(newname));
        EXPECT_TRUE(S_ISREG(st.st_mode));

        simple_nfs::File f(*nfs_,
                           newname,
                           youtils::FDMode::Write,
                           CreateIfNecessary::F);

        f.pwrite(newpattern.data(), newpattern.size(), 0);
    }

    nfs_->rename(oldname, newname);

    EXPECT_THROW(nfs_->stat(oldname),
                 simple_nfs::Exception);

    {
        const struct stat st(nfs_->stat(newname));
        EXPECT_TRUE(S_ISREG(st.st_mode));

        simple_nfs::File f(*nfs_,
                           newname,
                           youtils::FDMode::Read,
                           CreateIfNecessary::F);

        std::vector<char> rbuf(oldpattern.size());
        f.pread(rbuf.data(), rbuf.size(), 0);
        std::string s(rbuf.data(), rbuf.size());

        EXPECT_EQ(oldpattern, s);
    }
}

namespace
{

class NodeDataFile;
class NodeDataDirectory;
class NodeDataVolume;

class NodeData
{
public:
    typedef boost::property_tree::basic_ptree<std::string, std::shared_ptr<NodeData> > data_ptree;

    NodeData(simple_nfs::Mount& nfs,
             std::mt19937& gen)
        : nfs_(nfs)
        , gen_(gen)
    {}

    virtual ~NodeData()
    {}

    virtual void
    check(data_ptree&,
          const fs::path&) = 0;

    virtual void
    add_content(data_ptree&,
                const fs::path&) = 0;


    enum class FType
    {
        Directory,
        File,
        Volume
    };

    virtual void
    breakdown(data_ptree&,
              const fs::path&) = 0;

    virtual FType
    getType() const = 0;

    std::string pseudo_uuid(size_t length)
    {
        auto randchar = []() -> char
        {
            const char charset[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
            const size_t max_index = (sizeof(charset) - 1);
            return charset[rand() % max_index];
        };
        std::string str(length, 0);
        std::generate_n(str.begin(), length, randchar);
        return str;
    }

    simple_nfs::Mount& nfs_;
    std::mt19937& gen_;
};

class NodeDataFileOrDirectory : public NodeData
{
public:
    NodeDataFileOrDirectory(simple_nfs::Mount& nfs,
                            std::mt19937& gen,
                            char content_start_char,
                            char content_end_char,
                            double probability_to_add_content,
                            size_t min_size,
                            size_t max_size)
        : NodeData(nfs,
                    gen)
        , content_start_char_(content_start_char)
        , content_end_char_(content_end_char)
        , probability_to_add_content_(probability_to_add_content)
        , min_size_(min_size)
        , max_size_(max_size)
    {
        ASSERT(content_start_char < content_end_char);
        ASSERT(probability_to_add_content > 0);
    }

    virtual ~NodeDataFileOrDirectory()
    {}

    char
    content()
    {
        static std::uniform_int_distribution<char> char_dist('A', 'Z');
        return char_dist(gen_);
    }

    size_t
    written_size() const
    {
        size_t res = 0;

        for(const auto& i : content_)
        {
            res += i.second;
        }
        return res;
    }

    std::list<std::pair<char, size_t> > content_;

    bool
    add_content_p()
    {
        static std::discrete_distribution<int>
            add_content_distribution(std::initializer_list<double>(
                {1.0 - probability_to_add_content_,
                        probability_to_add_content_}));
        return add_content_distribution(gen_);
    }

    size_t
    size_to_write() const
    {
        std::uniform_int_distribution<int> size_dist(min_size_, max_size_);
        return size_dist(gen_);
    }


    const char content_start_char_;
    const char content_end_char_;
    const double probability_to_add_content_;
    const size_t min_size_;
    const size_t max_size_;

};



class NodeDataFile
    : public NodeDataFileOrDirectory
{
public:
    NodeDataFile(simple_nfs::Mount& nfs,
                 std::mt19937& gen,
                 data_ptree& ptree,
                 const fs::path& s_path)
        : NodeDataFileOrDirectory(nfs,
                                  gen,
                                  'A',
                                  'Z',
                                  0.25,
                                  88,
                                  88)
    {
        const std::string filename = pseudo_uuid(36);
        simple_nfs::File nfs_file(nfs_,
                                  s_path / filename,
                                  youtils::FDMode::ReadWrite,
                                  CreateIfNecessary::T);
        ptree.add_child(boost::property_tree::path(filename,'/'),
                        data_ptree(std::shared_ptr<NodeData>(this)));
    }

    virtual ~NodeDataFile()
    {}

    virtual FType
    getType() const override final
    {
        return FType::File;
    }

    virtual void
    breakdown(data_ptree& pt,
              const fs::path& s_path)  override
    {
        check(pt,
              s_path);
        simple_nfs::File nfs_file(nfs_,
                                  s_path,
                                  youtils::FDMode::ReadWrite,
                                  CreateIfNecessary::F);

        struct stat stat = nfs_file.stat();
        ASSERT_TRUE(S_ISREG(stat.st_mode));

        ASSERT_EQ(stat.st_size, static_cast<ssize_t>(written_size()));
        nfs_.unlink(s_path);
    }

    virtual void
    check(data_ptree& pt,
          const fs::path& s_path) override final
    {
        size_t start = 0;
        simple_nfs::File nfs_file (nfs_,
                                   s_path,
                                   youtils::FDMode::ReadWrite,
                                   CreateIfNecessary::F);

        for(const auto& i :  content_)
        {
            size_t length = i.second;
            char content = i.first;
            (void) content;
            std::vector<char> buf(length);
            size_t res = nfs_file.pread(buf.data(),
                                        length,
                                        start);
            ASSERT_EQ(res, length);
            for( const auto i : buf)
            {
                ASSERT_EQ(content, i);
            }
            start += length;
        }

        if(add_content_p())
        {
            add_content(pt,
                        s_path);
        }
    }

    virtual void
    add_content(data_ptree& /*pt*/,
                const fs::path& s_path) override final
    {
        size_t sz = size_to_write();
        char cont = content();
        std::vector<char> buf(sz, cont);
        int res = 0;
        simple_nfs::File nfs_file (nfs_,
                                   s_path,
                                   youtils::FDMode::ReadWrite,
                                   CreateIfNecessary::F);

        ASSERT_NO_THROW(res = nfs_file.pwrite(buf.data(),
                                              sz,
                                              written_size()));
        ASSERT_EQ(res, static_cast<ssize_t>(sz));
        content_.emplace_back(cont, sz);
    }
};

class NodeDataVolume
    : public NodeDataFileOrDirectory
{

public:
    NodeDataVolume(simple_nfs::Mount& nfs,
                   std::mt19937& gen,
                   data_ptree& ptree,
                   const fs::path s_path)
        : NodeDataFileOrDirectory(nfs,
                                  gen,
                                  'a',
                                  'z',
                                  0.50,
                                  4096,
                                  4096)
        , buf_(buf_size_)
    {
        const std::string volname = pseudo_uuid(36);
        fs::path path = volumedriverfstest::FileSystemTestSetup::make_volume_name(volname);

        std::unique_ptr<simple_nfs::File> nfs_volume;

        nfs_volume.reset(new simple_nfs::File(nfs_,
                                              s_path / path,
                                              youtils::FDMode::ReadWrite,
                                              CreateIfNecessary::T));


        volume_size_ = 10 * 1024 * 1024;
        nfs_volume->truncate(volume_size_);

        ptree.add_child(boost::property_tree::path(path.string(),'/'),
                        data_ptree(std::shared_ptr<NodeData>(this)));
    }

    virtual ~NodeDataVolume()
    {}

    virtual FType
    getType() const override final
    {
        return FType::Volume;
    }

    virtual void
    check(data_ptree& pt,
          const fs::path& s_path) override
    {
        simple_nfs::File nfs_file (nfs_,
                                   s_path,
                                   youtils::FDMode::ReadWrite,
                                   CreateIfNecessary::F);
        size_t start = 0;

        for(const auto& i :  content_)
        {
            size_t length = i.second;
            char content = i.first;
            size_t res = nfs_file.pread(buf_.data(),
                                        length,
                                        start);
            ASSERT_EQ(res, length);
            for( const auto i : buf_)
            {
                ASSERT_EQ(content, i);
            }

            start += length;
        }

        add_content(pt,
                    s_path);
    }

    virtual void
    add_content(data_ptree& /*pt*/,
                const fs::path& s_path) override
    {
        if(add_content_p())
        {

            simple_nfs::File nfs_file (nfs_,
                                       s_path,
                                       youtils::FDMode::ReadWrite,
                                       CreateIfNecessary::F);

            char cont = content();
            size_t written = written_size();

            if(written < volume_size_)
            {
                buf_.assign(buf_size_, cont);
                size_t res;
                ASSERT_NO_THROW(res = nfs_file.pwrite(buf_.data(),
                                                      buf_size_,
                                                      written));
                ASSERT_EQ(res, buf_size_);

                content_.emplace_back(cont, buf_size_);
            }
        }

        // overwrite NOT HERE YET
    }

    void
    breakdown(data_ptree& pt,
              const fs::path& s_path) override
    {
        check(pt,
              s_path);
        simple_nfs::File nfs_file (nfs_,
                                   s_path,
                                   youtils::FDMode::ReadWrite,
                                   CreateIfNecessary::F);

        struct stat stat = nfs_file.stat();

        ASSERT_TRUE(S_ISREG(stat.st_mode));

        ASSERT_EQ(stat.st_size, static_cast<ssize_t>(volume_size_));
        nfs_.unlink(s_path);
    }

    size_t volume_size_;
    const size_t buf_size_ = { 4096 };
    std::vector<char> buf_;
};

class NodeDataDirectory
    : public NodeData
{
public:
    NodeDataDirectory(simple_nfs::Mount& nfs,
                      std::mt19937& gen,
                      const fs::path& s_path,
                      size_t& number_of_entities)
        : NodeData(nfs,
                   gen)
        , number_of_entities_(number_of_entities)
    {
        if(not s_path.empty())
        {
            nfs_.mkdir(s_path);
        }
    }

private:
    NodeDataDirectory(simple_nfs::Mount& nfs,
                      std::mt19937& gen,
                      data_ptree& ptree,
                      const fs::path& s_path,
                      size_t& number_of_entities)
        : NodeData(nfs,
                    gen)
        , number_of_entities_(number_of_entities)
    {
        const std::string dirname = pseudo_uuid(36);
        nfs_.mkdir(s_path / dirname);

        ptree.put_child(boost::property_tree::path(dirname,'/'),
                        data_ptree(std::shared_ptr<NodeData>(this)));
    }

public:
    virtual ~NodeDataDirectory()
    {}

    virtual FType
    getType() const override final
    {
        return FType::Directory;
    }

    virtual void
    check(data_ptree& pt,
          const fs::path& s_path) override
    {
        for(auto i = pt.begin(); i != pt.end(); ++i)
        {
            i->second.data()->check(i->second,
                                    s_path / i->first);
        }
        add_content(pt,
                    s_path);
    }

    virtual void
    add_content(data_ptree& pt,
                const fs::path& s_path) override
    {
        switch(file_type_to_create())
        {
        case FType::Directory:
            ASSERT_NO_THROW(new NodeDataDirectory(nfs_,
                                                  gen_,
                                                  pt,
                                                  s_path,
                                                  number_of_entities_));

            ++number_of_entities_;
            return;

        case FType::File:
            ASSERT_NO_THROW(new NodeDataFile(nfs_,
                                             gen_,
                                             pt,
                                             s_path));

            ++number_of_entities_;
            return;

        case FType::Volume:
            ASSERT_NO_THROW(new NodeDataVolume(nfs_,
                                               gen_,
                                               pt,
                                               s_path));
            ++number_of_entities_;
            return;
        }
    }

    void
    breakdown(data_ptree& pt,
              const fs::path& s_path) override
    {
        {
            std::list<simple_nfs::DirectoryEntry> dir_entries(nfs_.readdir(s_path));
            size_t dir_entries_size = dir_entries.size();

            for(auto i = pt.begin(); i != pt.end(); ++i)
            {
                const std::string& name = i->first;
                auto pred =  [&name] (const simple_nfs::DirectoryEntry& entry) -> bool
                    {
                        return entry.name == name;
                    };


                dir_entries.remove_if(pred);

                ASSERT_EQ(--dir_entries_size, dir_entries.size());

                i->second.data()->breakdown(i->second,
                                            s_path / name);
            }
        }

        const std::list<simple_nfs::DirectoryEntry> dir_entries(nfs_.readdir(s_path));
        ASSERT_TRUE(dir_entries.empty());
        struct stat stat = nfs_.stat(s_path);

        ASSERT_TRUE(S_ISDIR(stat.st_mode));

        static const fs::path root_("/");

        if(s_path != root_)
        {
            ASSERT_NO_THROW(nfs_.rmdir(s_path));
        }
    }

    FType
    file_type_to_create()
    {
        const double chance_to_create_a_file = 0.90;
        const double chance_to_create_a_volume = 0.007;
        const double chance_to_create_a_directory = 1.0 - chance_to_create_a_file - chance_to_create_a_volume;
        static std::discrete_distribution<int>
            file_type_dist(std::initializer_list<double>({ chance_to_create_a_file,
                            chance_to_create_a_volume,
                            chance_to_create_a_directory }));
        switch(file_type_dist(gen_))
        {
        case 0:
            return FType::File;
        case 1:
            return FType::Volume;
        case 2:
            return FType::Directory;
        default:
            UNREACHABLE;
        }
    }

    size_t& number_of_entities_;
};

}

TEST_F(SimpleTest, torture)
{
    uint64_t number_of_stuff_to_create = 128;
    std::mt19937 gen(time(nullptr));
    size_t number_of_entities = 0;
    auto ndptr = std::shared_ptr<NodeData>(new NodeDataDirectory(*nfs_,
                                                                 gen,
                                                                 fs::path(),
                                                                 number_of_entities));

    NodeData::data_ptree ptree(ndptr);

    while(number_of_entities < number_of_stuff_to_create)
    {
        ptree.data()->check(ptree,
                            "/");
    }
    ptree.data()->breakdown(ptree,
                            "/");
    ASSERT_TRUE(nfs_->readdir("/").empty());
}

class FileSystemChecker
{
public:
    FileSystemChecker(const fs::path& dir,
                      simple_nfs::Mount& nfs,
                      const size_t number_of_stuff_to_create)
        : dir_(dir)
        , nfs_(nfs)
        , number_of_stuff_to_create_(number_of_stuff_to_create)
        , gen_(time(nullptr))
    {}

    void
    operator()()
    {

        size_t number_of_entities = 0;
        auto ndptr = std::shared_ptr<NodeData>(new NodeDataDirectory(nfs_,
                                                                     gen_,
                                                                     dir_,
                                                                     number_of_entities));
        NodeData::data_ptree ptree(ndptr);

        while(number_of_entities < number_of_stuff_to_create_)
        {
            ptree.data()->check(ptree,
                                dir_);
        }
        ptree.data()->breakdown(ptree,
                                dir_);

    }
private:
    fs::path dir_;
    simple_nfs::Mount& nfs_;
    const size_t number_of_stuff_to_create_;
    std::mt19937 gen_;

};


TEST_F(SimpleTest, multi_threaded_torture)
{
    uint64_t number_of_stuff_to_create = 128;

    simple_nfs::Mount nfs1(nfs_server(),
                           nfs_export());

    FileSystemChecker f_checker1(fs::path("/dir1"),
                                 nfs1,
                                 number_of_stuff_to_create);

    simple_nfs::Mount nfs2(nfs_server(),
                           nfs_export());

    FileSystemChecker f_checker2(fs::path("/dir2"),
                                 nfs2,
                                 number_of_stuff_to_create);
    simple_nfs::Mount nfs3(nfs_server(),
                           nfs_export());

    FileSystemChecker f_checker3(fs::path("/dir3"),
                                 nfs3,
                                 number_of_stuff_to_create);
    simple_nfs::Mount nfs4(nfs_server(),
                           nfs_export());

    FileSystemChecker f_checker4(fs::path("/dir4"),
                                 nfs4,
                                 number_of_stuff_to_create);

    boost::thread t1(boost::ref(f_checker1));
    boost::thread t2(boost::ref(f_checker2));
    boost::thread t3(boost::ref(f_checker3));
    boost::thread t4(boost::ref(f_checker4));

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    ASSERT_TRUE(nfs_->readdir("/").empty());
}

TEST_F(SimpleTest, DISABLED_setup_hack)
{
    std::promise<bool> p;
    std::future<bool> f(p.get_future());

    yt::SignalSet sigs{ SIGINT, SIGTERM };

    yt::SignalThread t(sigs,
                       [&p](int signal)
                       {
                           LOG_INFO("Got signal " << signal);
                           p.set_value(true);
                       });

    f.wait();
}

}
