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

#include <boost/lexical_cast.hpp>

#include "BackendTestBase.h"

#include <youtils/DimensionedValue.h>
#include <youtils/FileUtils.h>
#include <youtils/System.h>

#include <backend/BackendConfig.h>
#include <backend/BackendException.h>
#include <backend/BackendInterface.h>
#include <backend/S3Config.h>

namespace backend
{

namespace fs = boost::filesystem;

class BackendObjectTest
    : public BackendTestBase
{
public:
    BackendObjectTest()
        : BackendTestBase("BackendObjectTest")
    {}

    void
    SetUp() override final
    {
        BackendTestBase::SetUp();
        nspace_ = make_random_namespace();
    }

    void
    TearDown() override final
    {
        nspace_.reset();
        BackendTestBase::TearDown();
    }

    void
    ensureObjectDoesNotExist(const fs::path& p,
                             const std::string& objname)
    {
        EXPECT_FALSE(fs::exists(p));

        EXPECT_FALSE(bi_(nspace_->ns())->objectExists(objname));

        EXPECT_THROW(bi_(nspace_->ns())->getSize(objname),
                     BackendObjectDoesNotExistException);

        if (BackendTestSetup::backend_config().backend_type.value() == BackendType::S3)
        {
            EXPECT_THROW(bi_(nspace_->ns())->getCheckSum(objname),
                         BackendNotImplementedException);
        }
        else
        {
            EXPECT_THROW(bi_(nspace_->ns())->getCheckSum(objname),
                         BackendObjectDoesNotExistException);
        }
        EXPECT_THROW(bi_(nspace_->ns())->read(p, objname, InsistOnLatestVersion::T),
                     BackendObjectDoesNotExistException);

        EXPECT_FALSE(fs::exists(p));
    }

    void
    createAndListObjects(size_t objsize,
                         const std::string& pattern,
                         size_t num_objs,
                         size_t check_every_n = 0)
    {
        std::list<std::string> objs;

        const fs::path src(path_ / "test.src");
        const yt::CheckSum chksum(createTestFile(src,
                                                 objsize,
                                                 pattern));


        for (size_t i = 0; i < num_objs; ++i)
        {
            if (check_every_n != 0 and i % check_every_n == 0)
            {
                bi_(nspace_->ns())->listObjects(objs);
                EXPECT_EQ(i, objs.size());
                objs.clear();
            }

            std::stringstream ss;
            ss << std::setw(8) << std::setfill('0') << i;

            bi_(nspace_->ns())->write(src,
                                      ss.str(),
                                      OverwriteObject::F,
                                      &chksum);
        }

        bi_(nspace_->ns())->listObjects(objs);
        EXPECT_EQ(num_objs, objs.size());
    }

    size_t
    getListEntriesPerChunk() const
    {
        const BackendConfig& cfg = backend_config();

        const S3Config* s3cfg = dynamic_cast<const S3Config*>(&cfg);
        if (s3cfg != nullptr)
        {
            return 1000;
        }

        return 200;
    }

protected:
    std::unique_ptr<WithRandomNamespace> nspace_;
};

TEST_F(BackendObjectTest, basics)
{
    const std::string objname("test");
    const fs::path src(path_ / std::string(objname + ".src"));
    const fs::path dst(path_ / std::string(objname + ".dst"));

    ensureObjectDoesNotExist(dst,
                             objname);

    const std::string pattern("William, it was really nothing");

    const size_t size = 1 << 20;
    const yt::CheckSum chksum(createPutAndVerify(src,
                                                 size,
                                                 pattern,
                                                 cm_,
                                                 objname,
                                                 nspace_->ns(),
                                                 OverwriteObject::F));

    EXPECT_TRUE(retrieveAndVerify(dst,
                                  size,
                                  pattern,
                                  &chksum,
                                  cm_,
                                  objname,
                                  nspace_->ns()));

    bi_(nspace_->ns())->remove(objname);
    fs::remove_all(dst);

    ensureObjectDoesNotExist(dst, objname);
}

TEST_F(BackendObjectTest, overwrite)
{
    const std::string objname("test");
    const fs::path src(path_ / std::string(objname + ".src"));
    const fs::path dst(path_ / std::string(objname + ".dst"));

    const size_t size1 = 4 << 20;
    const std::string pattern1("Stop me if you think you've heard this one before");

    const yt::CheckSum chksum1(createPutAndVerify(src,
                                                  size1,
                                                  pattern1,
                                                  cm_,
                                                  objname,
                                                  nspace_->ns(),
                                                  OverwriteObject::F));

    EXPECT_TRUE(retrieveAndVerify(dst,
                                  size1,
                                  pattern1,
                                  &chksum1,
                                  cm_,
                                  objname,
                                  nspace_->ns()));

    fs::remove_all(src);
    fs::remove_all(dst);

    {
        const size_t size2 = size1 / 2;
        const std::string pattern2("Shoplifters of the world unite");
        const yt::CheckSum chksum2(createTestFile(src,
                                                  size2,
                                                  pattern2));
        ALWAYS_CLEANUP_FILE(src);
        EXPECT_THROW(bi_(nspace_->ns())->write(src,
                                               objname,
                                               OverwriteObject::F,
                                               &chksum2),
                     BackendOverwriteNotAllowedException);

        try
        {
            EXPECT_FALSE(chksum2 == bi_(nspace_->ns())->getCheckSum(objname));
            EXPECT_TRUE(chksum1 == bi_(nspace_->ns())->getCheckSum(objname));
        }
        catch(const BackendNotImplementedException& e)
        {}

        EXPECT_EQ(size1,
                  bi_(nspace_->ns())->getSize(objname));

        bi_(nspace_->ns())->read(dst,
                                 objname,
                                 InsistOnLatestVersion::T);
        ALWAYS_CLEANUP_FILE(dst);

        EXPECT_TRUE(retrieveAndVerify(dst,
                                      size1,
                                      pattern1,
                                      &chksum1,
                                      cm_,
                                      objname,
                                      nspace_->ns()));
    }

    size_t size2 = size1 / 2;
    const std::string pattern2("Bigmouth strikes again");
    const yt::CheckSum chksum2(createPutAndVerify(src,
                                                  size2,
                                                  pattern2,
                                                  cm_,
                                                  objname,
                                                  nspace_->ns(),
                                                  OverwriteObject::T));

    EXPECT_TRUE(retrieveAndVerify(dst,
                                  size2,
                                  pattern2,
                                  &chksum2,
                                  cm_,
                                  objname,
                                  nspace_->ns()));

}

TEST_F(BackendObjectTest, listTiny)
{
    const size_t objsize = 64 << 10;

    const std::string pattern("You just haven't earned it yet, Baby");
    createAndListObjects(objsize,
                         pattern,
                         100);
}


TEST_F(BackendObjectTest, listSmall)
{
    const size_t num_objs = getListEntriesPerChunk();
    const size_t objsize = 64 << 10;

    const std::string pattern("You just haven't earned it yet, Baby");
    createAndListObjects(objsize,
                         pattern,
                         num_objs);
}

TEST_F(BackendObjectTest, listBig)
{
    const size_t list_entries_per_chunk = getListEntriesPerChunk();
    const size_t num_objs = list_entries_per_chunk * 2 + 1;
    const size_t objsize = 4 << 10;

    const std::string pattern("Please, please, please let me get what I want");
    createAndListObjects(objsize,
                         pattern,
                         num_objs,
                         list_entries_per_chunk);
}

TEST_F(BackendObjectTest, getalot)
{
    const size_t ocount(yt::System::get_env_with_default("NUM_OBJECTS",
                                                         4));

    const std::string osizestr(yt::System::get_env_with_default("OBJECT_SIZE",
                                                                std::string("4MiB")));
    const yt::DimensionedValue osize(osizestr);

    const size_t iterations_per_obj(yt::System::get_env_with_default("ITERATIONS_PER_OBJECT",
                                                                     32));
    std::vector<std::string> objs;
    objs.reserve(ocount);

    for (size_t i = 0; i < ocount; ++i)
    {
        objs.emplace_back(std::string("object") + boost::lexical_cast<std::string>(i));
    }

    {
        std::vector<std::shared_future<void>> futures;
        futures.reserve(objs.size());

        for (const auto& o : objs)
        {
            futures.emplace_back(std::async(std::launch::async,
                                            [&]
                                            {
                                                EXPECT_NO_THROW(createAndPut(path_ / o,
                                                                             osize.getBytes(),
                                                                             o,
                                                                             cm_,
                                                                             o,
                                                                             nspace_->ns(),
                                                                             OverwriteObject::F));
                                            }));
        }

        for (auto& f: futures)
        {
            f.get();
        }
    }

    std::vector<std::shared_future<void>> futures;
    futures.reserve(objs.size());

    for (const auto& o : objs)
    {
        futures.emplace_back(std::async(std::launch::async,
                                        [&]
                                        {
                                            auto bi = cm_->getConnection();
                                            for (size_t i = 0; i < iterations_per_obj; ++i)
                                            {
                                                EXPECT_NO_THROW(bi->read(nspace_->ns(),
                                                                         path_ / (o + ".retrieved"),
                                                                         o,
                                                                         InsistOnLatestVersion::T));
                                            }
                                        }));
    }

    for (auto& f : futures)
    {
        f.get();
    }
}

// These two are not too interesting on their own - their sole purpose is to be
// able to run them against a backend and then check the backend logs for the
// desired behaviour (cf. OVS-1046).
TEST_F(BackendObjectTest, DISABLED_dont_insist_on_latest_version)
{
    const std::string oname("dont-insist-on-latest");
    const fs::path src(path_ / (oname + ".src"));
    const fs::path dst(path_ / (oname + ".dst"));

    createAndPut(src,
                 4096,
                 oname,
                 cm_,
                 oname,
                 nspace_->ns(),
                 OverwriteObject::F);

    bi_(nspace_->ns())->read(dst,
                             oname,
                             InsistOnLatestVersion::F);
}

TEST_F(BackendObjectTest, DISABLED_insist_on_latest_version)
{
    const std::string oname("insist-on-latest");
    const fs::path src(path_ / (oname + ".src"));
    const fs::path dst(path_ / (oname + ".dst"));

    createAndPut(src,
                 4096,
                 oname,
                 cm_,
                 oname,
                 nspace_->ns(),
                 OverwriteObject::F);

    bi_(nspace_->ns())->read(dst,
                             oname,
                             InsistOnLatestVersion::T);
}

// This is not too interesting either; it's mainly a debugging / testing help
TEST_F(BackendObjectTest, cache_invalidation)
{
    const std::string oname("some-object");
    const fs::path src(path_ / (oname + ".src"));
    const fs::path dst(path_ / (oname + ".dst"));
    const size_t size = 1ULL << 20;

    BackendInterfacePtr bi(bi_(nspace_->ns()));

    const std::string pattern1("some pattern");

    const yt::CheckSum chksum1(createPutAndVerify(src,
                                                  size,
                                                  pattern1,
                                                  cm_,
                                                  oname,
                                                  nspace_->ns(),
                                                  OverwriteObject::F));

    auto check([&](const std::string& pattern,
                   const yt::CheckSum& chksum,
                   InsistOnLatestVersion insist)
               {
                   bi->read(dst,
                            oname,
                            insist);

                   ALWAYS_CLEANUP_FILE(dst);

                   EXPECT_TRUE(verifyTestFile(dst,
                                              size,
                                              pattern,
                                              &chksum));
               });

    check(pattern1,
          chksum1,
          InsistOnLatestVersion::F);

    const std::string pattern2("some other pattern");

    const yt::CheckSum chksum2(createAndPut(src,
                                            size,
                                            pattern2,
                                            cm_,
                                            oname,
                                            nspace_->ns(),
                                            OverwriteObject::T));

    check(pattern2,
          chksum2,
          InsistOnLatestVersion::T);

    bi->invalidate_cache();

    check(pattern2,
          chksum2,
          InsistOnLatestVersion::F);
}

TEST_F(BackendObjectTest, checksum_mismatch)
{
    const std::string oname("some-object");
    const fs::path opath(path_ / oname);

    const yt::CheckSum cs(createTestFile(opath,
                                         4096,
                                         "some pattern"));

    BackendInterfacePtr bi(bi_(nspace_->ns()));

    yt::CheckSum wrong_cs(1);

    ASSERT_NE(wrong_cs,
              cs);

    ASSERT_THROW(bi->write(opath,
                           oname,
                           OverwriteObject::F,
                           &wrong_cs),
                 BackendInputException);

    ASSERT_FALSE(bi->objectExists(oname));
}

}

// Local Variables: **
// mode: c++ **
// End: **
