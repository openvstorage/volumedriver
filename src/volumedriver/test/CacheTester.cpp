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

#include "ExGTest.h"
#include <youtils/SocketServer.h>
#include "Types.h"
#include <youtils/WrapByteArray.h>
#include "FailOverCacheBridge.h"
namespace volumedriver
{
namespace fs = boost::filesystem;
// Y42 see what needs to be rescued
/*
class CacheTester : public ExGTest
{
public:
    CacheTester()
        : port_(1456)
        , host_("127.0.0.1")
        , cacheSocketServer_(0)
        , path_(getTempPath("/cacheserver"))
    {}

    virtual void
    SetUp()
    {
        fs::remove_all(path_);
    }


    virtual void
    TearDown()
    {
        fs::remove_all(path_);
    }


    void
    startCacheServer()
    {
        if(not cacheSocketServer_)
        {
            create_directories(path_);
            // Y42 will take a path eventually
            cacheFact_.reset(new VolumeDriverCacheProtocolFactory(path_.file_string()));
            cacheSocketServer_ = fungi::SocketServer::createSocketServer(cacheFact_.get(), port_);
        }
        else
        {
            LOG_ERROR("Cache Server already Started");
            throw fungi::IOException("Cache Server already Started");
        }

    }
    DECLARE_LOGGER("CacheTester");

    void
    stopCacheServer()
    {
        if (cacheSocketServer_)
        {
            cacheSocketServer_->stop();
            cacheSocketServer_->join();
            delete cacheSocketServer_;
            cacheSocketServer_ = 0;
            cacheFact_.reset(0);
        }
        else
        {
            LOG_ERROR("Cache Server already Started");
            throw fungi::IOException("Cache Server already Started");
        }

    }

    RemoteVolumeDriverCache*
    newCache(const std::string& ns)
    {
        return new RemoteVolumeDriverCache(host_,
                                           port_,
                                           ns,
                                           FailOverWriter::RequestTimeout);
    }


protected:

    const uint16_t port_;
    const std::string host_;
    fungi::SocketServer *cacheSocketServer_;

    std::auto_ptr<VolumeDriverCacheProtocolFactory> cacheFact_;
    fs::path path_;
};

TEST_F(CacheTester, teststartstop1)
{
    startCacheServer();
    stopCacheServer();
}

TEST_F(CacheTester, teststartstop2)
{
    startCacheServer();
    std::auto_ptr<RemoteVolumeDriverCache> cache(newCache("ns"));
    stopCacheServer();

}

TEST_F(CacheTester, teststartstop3)
{
    startCacheServer();
    RemoteVolumeDriverCache* cache(newCache("ns"));
    delete cache;
    stopCacheServer();

}

class Test1Sink : public VolumeDriverCacheEntrySink
{
public:
    Test1Sink(const std::vector<std::vector<uint8_t> >& bufs)
        : bufs_(bufs),
          count_(0)
    {
    }


    virtual void
    processCacheEntry(int64_t lba,
                      const byte* buf,
                      int64_t size)
    {
        if(lba != count_ or
           size != (count_ * 16) or
           memcmp(buf, &bufs_[count_][0],size) != 0)
        {

            throw count_;
        }
        else
        {
            ++count_;
        }
    }

    unsigned
    getCount()
    {
        return count_;
    }

private:
    const std::vector<std::vector<uint8_t> >& bufs_;
    unsigned count_;
};


TEST_F(CacheTester, DISABLED_test1)
{
    startCacheServer();

    const unsigned sz = 10;
    std::vector<std::vector<uint8_t> > bufs;

    for(unsigned i = 0; i < sz; ++i)
    {
        bufs.push_back(std::vector<uint8_t>(i*16,static_cast<uint8_t>(i)));
    }
    std::auto_ptr<RemoteVolumeDriverCache> cache(newCache("ns"));

    for(unsigned i =0; i < 10; ++i)
    {
        fungi::WrapByteArray b(&bufs[i][0], bufs[i].size());
        //        cache->addEntry("tlog",i,&b);
    }
    cache->Flush();

    Test1Sink s(bufs);
    ASSERT_NO_THROW(cache->getEntries(s));
    EXPECT_EQ(s.getCount(),sz);
    stopCacheServer();

}


TEST_F(CacheTester, DISABLED_test2)
{
    startCacheServer();
    const unsigned sz = 10;
    std::vector<std::vector<uint8_t> > bufs;

    for(unsigned i = 0; i < sz; ++i)
    {
        bufs.push_back(std::vector<uint8_t>(i*16,static_cast<uint8_t>(i)));
    }
    {

        std::auto_ptr<RemoteVolumeDriverCache> cache(newCache("ns"));
        for(unsigned i =0; i < 10; ++i)
        {
            fungi::WrapByteArray b(&bufs[i][0], bufs[i].size());
            //            cache->addEntry("tlog",i,&b);
        }
        cache->Flush();
    }
    sleep(1);

    Test1Sink s(bufs);

    std::auto_ptr<RemoteVolumeDriverCache> cache(newCache("ns"));
    ASSERT_NO_THROW(cache->getEntries(s));
    EXPECT_EQ(s.getCount(),sz);
    stopCacheServer();
}

TEST_F(CacheTester, DISABLED_test3)
{
    startCacheServer();
    const unsigned sz = 10;
    std::vector<std::vector<uint8_t> > bufs;

    for(unsigned i = 0; i < sz; ++i)
    {
        bufs.push_back(std::vector<uint8_t>(i*16,static_cast<uint8_t>(i)));
    }
    std::auto_ptr<RemoteVolumeDriverCache> cache(newCache("ns"));
    for(unsigned i =0; i < 10; ++i)
    {
        fungi::WrapByteArray b(&bufs[i][0], bufs[i].size());
        // cache->addEntry("tlog",i,&b);

    }
    cache->Flush();
    Test1Sink s(bufs);

    ASSERT_NO_THROW(cache->getEntries(s));
    EXPECT_EQ(s.getCount(),10);
    stopCacheServer();

}
*/
}

// Local Variables: **
// mode: c++ **
// End: **
