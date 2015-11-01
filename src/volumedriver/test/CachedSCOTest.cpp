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

#include <boost/thread.hpp>

#include "SCOCacheTestSetup.h"
#include "../CachedSCO.h"
#include "../OpenSCO.h"

namespace volumedriver
{

namespace yt = youtils;
using namespace initialized_params;

class CachedSCOTest
    : public SCOCacheTestSetup
{

public:
    void
    chunkyRead(CachedSCOPtr sco,
               size_t chunksize,
               const std::vector<byte>& ref,
               size_t iterations = 1)
    {
        const size_t scosize = sco->getSize();
        EXPECT_EQ(0U,  scosize % chunksize) << "Fix your test";

        for (size_t i = 0; i < iterations; ++i)
        {
            std::vector<byte> buf(scosize);
            {
                for (size_t j = 0; j < scosize; j += chunksize)
                {
                    OpenSCOPtr osco(sco->open(FDMode::Read));
                    osco->pread(&buf[j], chunksize, j);
                }
            }

            EXPECT_TRUE(ref == buf);
        }
    }


protected:
    virtual void
    SetUp()
    {
        throttle_usecs = 1000;
        SCOCacheTestSetup::SetUp();

        mp_path_ = pathPfx_ / "mp1";
        mp_size_ = 1 << 29; // 512 MiB

        fs::create_directories(mp_path_);

        MountPointConfigs mpcfgs;

        mpcfgs.push_back(MountPointConfig(mp_path_,
                                          mp_size_));

        boost::property_tree::ptree pt;
        pt.put("version", 1);

        PARAMETER_TYPE(scocache_mount_points)(mpcfgs).persist(pt);

        PARAMETER_TYPE(datastore_throttle_usecs)(throttle_usecs).persist(pt);
        PARAMETER_TYPE(trigger_gap)(yt::DimensionedValue(1)).persist(pt);
        PARAMETER_TYPE(backoff_gap)(yt::DimensionedValue(1)).persist(pt);

        scoCache_.reset(new SCOCache(pt));

        scoCache_->addNamespace(nspace_,
                                0,
                                std::numeric_limits<uint64_t>::max());

    }

    void
    stopSCOCache()
    {
        scoCache_.reset(0);
    }

    CachedSCOPtr
    createSCO(SCO scoName,
              uint64_t maxSize)
    {
        return scoCache_->createSCO(nspace_,
                                    scoName,
                                    maxSize);
    }

    bool
    isDisposable(CachedSCOPtr sco)
    {
        return sco->isDisposable();
    }

    void
    setDisposable(CachedSCOPtr sco)
    {
        sco->setDisposable();
    }

    fs::path mp_path_;
    uint64_t mp_size_;
    std::unique_ptr<SCOCache> scoCache_;

    std::atomic<uint32_t> throttle_usecs;
    const backend::Namespace nspace_;
};

TEST_F(CachedSCOTest, invalidSize)
{
    EXPECT_THROW(createSCO(SCO(1),
                           0),
                 fungi::IOException);
}

TEST_F(CachedSCOTest, createAndUseOne)
{
    uint64_t maxSize = 8 << 10;
    SCO scoName(0xdeadbeef);

    CachedSCOPtr sco(createSCO(scoName,
                               maxSize));

    EXPECT_EQ(nspace_, sco->getNamespace()->getName());
    EXPECT_EQ(maxSize, sco->getSize());
    EXPECT_EQ(scoName, sco->getSCO());

    uint32_t throttle;

    EXPECT_FALSE(isDisposable(sco));

    const char buf[] = "test";

    {
        OpenSCOPtr osco(sco->open(FDMode::Write));
        osco->pwrite(buf, sizeof(buf), 0, throttle);
        EXPECT_FALSE(throttle);
    }

    EXPECT_TRUE(fs::exists(sco->path()));

    struct stat st;
    int ret = ::stat(sco->path().string().c_str(),
                     &st);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE((st.st_mode & S_ISVTX));
    EXPECT_EQ(static_cast<long>(sizeof(buf)),
              st.st_size);

    setDisposable(sco);

    EXPECT_TRUE(isDisposable(sco));

    ret = ::stat(sco->path().string().c_str(),
                 &st);
    EXPECT_EQ(0, ret);
    EXPECT_TRUE((st.st_mode & S_ISVTX));
    EXPECT_EQ(static_cast<ssize_t>(sizeof(buf)),
              st.st_size);

    int cnt = 1024;
    for (int i = 0; i < cnt; ++i)
    {
        OpenSCOPtr osco(sco->open(FDMode::Write));
        osco->pwrite(&i, sizeof(i), i * sizeof(i), throttle);
        EXPECT_FALSE(throttle);
    }

    // removal is refcounted: only once the last reference to it is dropped the
    // sco is actually deleted
    const fs::path p(sco->path());

    scoCache_->removeSCO(sco->getNamespace()->getName(),
                         sco->getSCO(),
                         false);

    EXPECT_TRUE(fs::exists(p));
    sco = 0;
    EXPECT_FALSE(fs::exists(p));
}

TEST_F(CachedSCOTest, preadpwrite)
{
    ssize_t size = 8 << 10;
    SCO scoName(0xdeadbeef);

    CachedSCOPtr sco(createSCO(scoName,
                               size * 2));

    std::vector<byte> wbuf(size);
    std::vector<byte> rbuf(size);

    uint32_t throttle;
    {
        OpenSCOPtr osco(sco->open(FDMode::Write));
        EXPECT_EQ(size, osco->pwrite(&wbuf[0], size, 0, throttle));

        memset(&wbuf[0], 0xff, size);
        EXPECT_EQ(size, osco->pwrite(&wbuf[0], size, size, throttle));
        EXPECT_FALSE(throttle);
        osco->sync();
    }

    {
        OpenSCOPtr osco(sco->open(FDMode::Read));
        EXPECT_EQ(size, osco->pread(&rbuf[0], size, size));
        EXPECT_EQ(0, memcmp(&wbuf[0], &rbuf[0], size));

        memset(&wbuf[0], 0x0, size);
        EXPECT_EQ(size, osco->pread(&rbuf[0], size, 0));
        EXPECT_EQ(0, memcmp(&wbuf[0], &rbuf[0], size));
    }
}

TEST_F(CachedSCOTest, concurrentRead)
{
    size_t size = 8 << 20;
    SCO scoName(0xdeadbeef);

    CachedSCOPtr sco(createSCO(scoName,
                               size));

    const std::vector<byte> wbuf(size, 'x');
    {
        uint32_t throttle ;
        OpenSCOPtr osco(sco->open(FDMode::Write));
        osco->pwrite(&wbuf[0], size, 0, throttle);
        EXPECT_FALSE(throttle);
        osco->sync();
    }

    setDisposable(sco);

    const size_t iterations = 50;

    boost::thread t1(boost::bind(&CachedSCOTest::chunkyRead,
                                 this,
                                 sco,
                                 1024,
                                 wbuf,
                                 iterations));

    boost::thread t2(boost::bind(&CachedSCOTest::chunkyRead,
                                 this,
                                 sco,
                                 512,
                                 wbuf,
                                 iterations));

    t1.join();
    t2.join();
}

}

// Local Variables: **
// mode: c++ **
// End: **
