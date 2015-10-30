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

#include "VolManagerTestSetup.h"

#include "../Api.h"
#include "../CachedMetaDataPage.h"
#include "../DataStoreNG.h"
#include "../VolManager.h"

#include <cerrno>
#include <fstream>
#include <iostream>
#include <limits>
#include <regex>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>

#include <youtils/ArakoonNodeConfig.h>
#include <youtils/ArakoonInterface.h>
#include <youtils/cpu_timer.h>
#include <youtils/RWLock.h>
#include <youtils/StrongTypedString.h>
#include <youtils/System.h>
#include <youtils/wall_timer.h>

namespace volumedriver
{

namespace ara = arakoon;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace yt = youtils;

using namespace std::literals::string_literals;

class cases
    : public VolManagerTestSetup
{
public:
    cases()
        : VolManagerTestSetup("cases")
    {
        // dontCatch(); uncomment if you want an unhandled exception to cause a crash, e.g. to get a stacktrace

    }
    DECLARE_LOGGER("cases");
};

// No more XmlRpc for us
// ... never say never (or "no more").

//using namespace ::XmlRpc;

static const uint32_t sizew = 8192;
static const uint32_t num_writes= 1 << 14;
static const uint32_t distance = 16; // at least sizew over 512

TEST_P(cases, DISABLED_cacheserver1)
{
    Volume* v1 = newVolume("volume1",
                           backend::Namespace());

    v1->setFailOverCacheConfig(FailOverCacheConfig(FailOverCacheTestSetup::host(),
                                                   FailOverCacheTestSetup::port_base(),
                                                   GetParam().foc_mode()));

    ASSERT_TRUE(v1);
    LOG_INFO("Volume v1 Size: " << v1->getSize() / ((1024.0) * (1024.0)) << "MiB");


    for(unsigned i = 0; i < num_writes; i++)
    {
            writeToVolume(v1, i*distance, sizew, "X");
    }
    createSnapshot(v1,"snap1");
    while(not v1->isSyncedToBackend())
    {
        sleep(1);
    }
    Volume* v2 =  createClone("volume2",
                              backend::Namespace(),
                              backend::Namespace(),
                              "snap1");
    ASSERT_TRUE(v2);

    v2->setFailOverCacheConfig(FailOverCacheConfig(FailOverCacheTestSetup::host(),
                                                   45017,
                                                   GetParam().foc_mode()));

    const VolumeConfig cfg(v2->get_config());
    for(unsigned i = 0; i < num_writes; i++)
    {
        if(i%3 == 1)
        {
            writeToVolume(v2, i*distance, sizew, "Y");
        }
    }
    createSnapshot(v2,"snap1");
    while(not v2->isSyncedToBackend())
    {
        sleep(1);
    }

    {
        SCOPED_DESTROY_VOLUME_UNBLOCK_BACKEND(v2,
                                              2,
                                              DeleteLocalData::T,
                                              RemoveVolumeCompletely::F);
        for(unsigned i = 0; i < num_writes; ++i)
        {
            if(i%3 == 2)
            {
                writeToVolume(v2, i*distance, sizew, "Z");
            }

        }
    }
    v2 = 0;
    restartVolume(cfg);
    v2 = getVolume(VolumeId("volume2"));
    ASSERT_TRUE(v2);

    for(unsigned i = 0; i < num_writes; ++i)
    {
        switch(i %3 )
        {
        case 0:
            checkVolume(v2,i*distance, sizew, "X");
            break;
        case 1:
            checkVolume(v2,i*distance, sizew, "Y");
            break;
        case 2:
            checkVolume(v2,i*distance, sizew, "Z");
            break;
        default:
            assert(0=="remarkable");
        }
    }

    destroyVolume(v2,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

#if BART_WANTS_TODO_ANOTHER_ROUND_OF_XMLRPC_DEBUGGING
TEST_P(cases, DISABLED_fawltySnapshots)
{
    const std::string name("testvol");
    Volume* v = newVolume(name,
                          backend::Namespace());

    ::XmlRpc::XmlRpcValue args;
    args["rule_id"] = "1";
    args["path_regex"] = ".*/snapshots\\.xml";
    args["call_regex"] = "write";
    args["duration"] = "1000";
    args["ramp_up"] = "0";
    args["error_code"] = "5"; // EIO

    ::XmlRpc::XmlRpcValue res;
    ::XmlRpc::XmlRpcClient c("localhost", 23456);
    c.execute("addFailureRule", args, res);
    ASSERT_FALSE(c.isFault());

    BOOST_SCOPE_EXIT((&c))
    {
        ::XmlRpc::XmlRpcValue args;
        args["rule_id"] = "1";
        ::XmlRpc::XmlRpcValue res;
        c.execute("removeFailureRule", args, res);
    }
    BOOST_SCOPE_EXIT_END;

    auto i = 0;

    try
    {
        for (; i < 128; ++i)
        {
            createSnapshot(v, boost::lexical_cast<std::string>(i));
            waitForThisBackendWrite(v);
        }
    }
    catch (std::exception& e)
    {
        std::cout << "Exception " << e.what() << " after " << i << " snapshots" << std::endl;
    }

    EXPECT_TRUE(v->is_halted());
}

class ABigCall : public ::XmlRpc::XmlRpcServerMethod
{
public:
    ABigCall(const std::string& name,
             ::XmlRpc::XmlRpcServer* s)
        : ::XmlRpc::XmlRpcServerMethod(name,s)
    {}
    virtual void
    execute(::XmlRpc::XmlRpcValue& /*rams*/,
            ::XmlRpc::XmlRpcValue& result)1
    {
        for(int i = 0; i < 1024 * 1024; i++)
        {
            std::stringstream a;
            a << i;
            result[i] = a.str();
        }

    }
    std::string
    help()
    {
        return std::string("nohelp");
    }
};



class XMLRPCRunnable : public fungi::Runnable
{
public:
    XMLRPCRunnable(uint16_t port);


    void
    run();

    const char*
    getName() const
    {
        return "XmlRpcRunnable";
    }

    void
    stop();

private:
    std::auto_ptr< ::XmlRpc::XmlRpcServer> server_;
    fungi::Thread* xmlrpcthread_;
    unsigned xmlrpcport_;
    bool stop_;
};

XMLRPCRunnable::XMLRPCRunnable(uint16_t port)
    : xmlrpcport_(port)
    , stop_(false)
{
    if(xmlrpcport_ > 0)
    {
        server_.reset(new XmlRpc::XmlRpcServer());

        ABigCall* abc = new ABigCall("abc",server_.get());
        (void)abc;

        // Y42 do some error handling here + logging that is aligned with our logging
        XmlRpc::setVerbosity(0);
        stop_ = false;

        bool started = server_->bindAndListen(xmlrpcport_);
        if(not started)
        {
            throw fungi::IOException("Could not listen to the xmlrpcport");
        }

        server_->enableIntrospection(true);
        xmlrpcthread_ = new fungi::Thread(*this);
        xmlrpcthread_->start();
    }
}

void XMLRPCRunnable::run()
{

    while (!stop_) {
        server_->work(1.0);
    }
    server_->exit();
    server_->shutdown();
}


void
XMLRPCRunnable::stop()
{
    if(xmlrpcthread_)
    {
        stop_ = true;
        try {
            xmlrpcthread_->join();
            xmlrpcthread_->destroy();
        } catch (fungi::IOException &e) {
            LOG_ERROR("Failed to stop the XMLRPC thread: " << e.what());
        }
    }
}

TEST_P(cases, XMLRPCServer)
{
    XMLRPCRunnable xmlrpc(22000);
    // Please don’t wake me,no don’t shake me,
    // Leave me where I am, I’m only sleeping.
    // John Lennon.
    sleep(200000);
}

#endif

const unsigned num_tests = 50;

TEST_P(cases, DISABLED_restartALittle) // Ev'ry time you go away, I ...
{
    std::string t("bart");

    Volume* v1 = newVolume("vol1",
                           backend::Namespace());

    auto foc_ctx(start_one_foc());

    v1->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

    writeToVolume(v1,0,4096*2048, t);
    const VolumeConfig vCfg(v1->get_config());

    destroyVolume(v1,
                  DeleteLocalData::F,
                  RemoveVolumeCompletely::F);

    for( unsigned i = 0; i < num_tests; ++i)
    {
        v1 = 0;

        v1 = newVolume("vol1",
                       backend::Namespace());

        ASSERT_TRUE(v1);
        v1->setFailOverCacheConfig(foc_ctx->config(GetParam().foc_mode()));

        checkVolume(v1,0,4096*2048,t);

        std::stringstream ss;
        ss << "bart_" << i;
        t = ss.str();
        writeToVolume(v1,0,4096*2048, t);
        destroyVolume(v1,
                      DeleteLocalData::F,
                      RemoveVolumeCompletely::F);
    }
}

TEST_P(cases, DISABLED_multisnapshot)
{
  Volume* v1 = newVolume("vol1",
                         backend::Namespace());
    for(int i = 0; i < 512; ++i)
    {
        if(i % 20 == 0)
        {
            LOG_INFO("creating snapshot the " << i << "th");
        }

        std::stringstream ss;
        ss << i;

        //        writeToVolume(v1,0,4096,"arne");
        createSnapshot(v1,ss.str());
    }

    //    waitForThisBackendWrite(v1);
    //    waitForThisBackendWrite(v1);
    //    int previous = -1;

    for(int i = 0; i < 64; ++i)
    {
        if(i % 20 == 0)
        {
            LOG_INFO("getting work the " << i << "th");
        }

        // Work vwork;
        // api::getSnapshotScrubbingWork(VolumeId("vol1"), boost::none, boost::none, vwork);
        // ASSERT_TRUE((int)vwork.snapshots.size() >= previous);
        // previous = vwork.snapshots.size();

        // SnapshotWork::const_iterator it;
        // for(it = vwork.snapshots.begin(); it != vwork.snapshots.end(); ++it)
        // {
        //     std::stringstream ss(*it);
        //     int i = -1;
        //     ss >> i;
        //     // ASSERT_TRUE( (i < 1024 ) and (i >= 0));
        //     // ASSERT_TRUE(it->in.size() == 1);
        //     // ASSERT_TRUE(it->out.size() == 1);
        //     // ASSERT_TRUE(it->reloc.size() == 1);
        //     OrderedTLogNames out;
        //     v1->getSnapshotManagement().getTLogsInSnapshot(*it,
        //                                                    out,
        //                                                    AbsolutePath::F);
        //     //            ASSERT_TRUE(out[0] == it->in[0]);
        // }
    }
}

// TEST_P(cases, DISABLED_bartscrash)
// {
//     XmlRpcValue Args;
//     Args["clusterMultiplier"] = "8";
//     Args["dataCachePath"] = "/tmp/test/dc";
//     Args["datastoreScosPerDir"] = "1";
//     Args["datastoreSize"] = "30MiB";
//     Args["director"] = "192.168.12.125";
//     Args["failoverType"] = "Standalone";
//     Args["hasLocalBackend"] = "false";
//     Args["lbaSize"] = "512";
//     Args["metaDataPath"] = "/tmp/test/mc";
//     Args["metadatastoreCacheSize"] = "100";
//     Args["namespace"] = "imma_0";
//     Args["parentNamespace"] = "";
//     Args["parentSnapshot"] = "";
//     Args["port"] = "23500";
//     Args["scoMultiplier"] = "1024";
//     Args["tlogPath"] = "/tmp/test/tlogs";
//     Args["uniqueVolumeIdentifier"] = "test";
//     Args["volumeSize"] = "30GiB";
//     XmlRpcValue Res;
//     XmlRpcClient c("localhost", 28000);
//     c.execute("volumeCreate",Args, Res);
//     ASSERT_FALSE(c.isFault());
//     Volume* v;

//     v = getVolume(VolumeId("test"));
//     ASSERT_TRUE(v);
//     LOG_DEBUG("Starting the writes");

//     while(true)
//     {
//         writeToVolume(v,0, 4096,"bart");
//     }
// }

TEST_P(cases, DISABLED_sizes)
{
    LOG_INFO("Size of ClusterLocation: " << sizeof(ClusterLocation));
    LOG_INFO("Size of a page: " << CachePage::size());
}

TEST_P(cases, DISABLED_cacheserver)
{
    auto foc_ctx(start_one_foc());

    Volume* v1 = newVolume("vol",
                           backend::Namespace());

    sleep(15);
    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
    sleep(15);
}

TEST_P(cases, test2)
{
    auto ns_ptr = make_random_namespace();
    Volume* v = newVolume("1volume",
                          ns_ptr->ns());
    const int numwrites = 128;

    for(int i = 0; i < 1; ++i)
    {
        writeToVolume(v,
                      0,
                      16384,
                      "abc");
    }

    for(int i = 0; i < numwrites; ++i)
    {
        checkVolume(v,
                0,
                4096,
                "abc");
    }

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
}

TEST_P(cases, DISABLED_test3)
{
    auto ns_ptr = make_random_namespace();
    Volume* v = newVolume("1volume",
                          ns_ptr->ns());

    writeToVolume(v,
                  0,
                  16384,
                  "abc");
    checkVolume(v,
                0,
                16384,
                "abc");
    temporarilyStopVolManager();
    startVolManager();

    Volume *v1 = getVolume(VolumeId("1volume"));
    ASSERT_TRUE(v1);
    checkVolume(v1,
                0,
                16384,
                "abc");
    //v1->put();
    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);


}

TEST_P(cases, DISABLED_startup)
{}


TEST_P(cases, DISABLED_bartNonLocalRestart)
{
    //    startcacheserver();
    auto ns_ptr = make_random_namespace();
    Volume* v1 = newVolume("vol1",
                           ns_ptr->ns());


    const VolumeConfig v1cfg(v1->get_config());

    auto ns2_ptr = make_random_namespace();
    Volume* v2 = newVolume("vol2",
                           ns2_ptr->ns());

    const int mult = v1->getClusterSize()/ v1->getLBASize();
    for(int i = 0; i < 1000; i++)
    {
        writeToVolume(v1,
                      i*mult,
                      v1->getClusterSize(),
                      "immanuel");
        writeToVolume(v2,
                      i*mult,
                      v1->getClusterSize(),
                      "immanuel");

    }

    v1->createSnapshot("snap1");

    for(int i = 0; i < 100; i++)
    {
        writeToVolume(v1,
                      i*mult,
                      v1->getClusterSize(),
                      "bart");
        writeToVolume(v2,
                      i*mult,
                      v2->getClusterSize(),
                      "bart");

    }
    EXPECT_TRUE(checkVolumesIdentical(v1, v2));


    scheduleBackendSync(v1);


    while(not isVolumeSyncedToBackend(v1))
    {
    }



    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    restartVolume(v1cfg);
    v1 = 0;

    v1 = getVolume(VolumeId("vol1"));
    ASSERT_TRUE(v1);

    EXPECT_TRUE(checkVolumesIdentical(v1,v2));

    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);
    destroyVolume(v2,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);

}



/*
TEST_P(cases, DISABLED_kristafClones3)
{
    Volume* v1 = newVolume("1volume",
                           backend::Namespace("openvstorage-volumedrivertest-namespace1"));

    const int mult = v1->getClusterSize()/ v1->getLBASize();

    for(int i = 0; i < 100; i++)
    {

        writeToVolume(v1,
                      i*mult,
                      v1->getClusterSize(),
                      "immanuel");
    }

    v1->createSnapshot("snap1");
    waitForThisBackendWrite(v1);
    waitForThisBackendWrite(v1);

    Volume* v2 = createClone("2volume",
                             backend::Namespace("namespace2"),
                             backend::Namespace("namespace1"),
                             "snap1");

    ASSERT_TRUE(checkVolumesIdentical(v1,
                                      v2));

    waitForThisBackendWrite(v1);
    waitForThisBackendWrite(v1);
    //    destroyVolume(v1,true,false);


    for(int i = 0 ; i < 100; i++)
    {
        writeToVolume(v2,
                      i*mult,
                      v1->getClusterSize(),
                      "bart");
    }

    v2->createSnapshot("snap2");

    Volume* v3 = createClone("3volume",
                             backend::Namespace("namespace3"),
                             backend::Namespace("namespace2"),
                             "snap1");

    for(int i = 0 ; i < 100; i++)
    {
        writeToVolume(v3,
                      i*mult,
                      v1->getClusterSize(),
                      "bart");
    }

    destroyVolume(v2,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

}

// a simple micro benchmark, generally not of interest for test suite runs
TEST_P(cases, DISABLED_DataStorePerformanceClustered)
{
    size_t count = 10;
    char *countEnv = getenv("VD_TEST_DSTORE_IO_ITERATIONS");
    if (countEnv) {
        count = strtoul(countEnv, NULL, 0);
        if (count == ULONG_MAX) {
            FAIL() << "cannot convert " << count << " to unsigned long: " <<
                strerror(errno);
        }
    }

    Volume *vol = newVolume("1volume",
                            backend::Namespace("openvstorage-volumedrivertest-namespace1"));


    SCOPED_BLOCK_BACKEND(vol);

    size_t num_clusters = 8;

    size_t bufSize = vol->getClusterSize();
    std::vector<uint8_t> bufv(bufSize);
    std::vector<std::vector<ClusterLocation> > locv(count / num_clusters);

    for (size_t i = 0; i < count / num_clusters; ++i)
    {
        locv[i].resize(num_clusters);
    }

    DataStoreNG* dStore = vol->getDataStore();

    size_t clusters = 0;
    {
        youtils::wall_timer timer;


        uint32_t throttle;

        for (size_t i = 0; i < count / num_clusters; ++i)
        {

            dStore->writeClusters(&bufv[0],
                                  locv[i],
                                  locv[i].size(),
                                  throttle);

            clusters += locv[i].size();
        }
        //        interval.stop();
        LOG_FATAL("Wrote " << clusters << " clusters in " <<
                  timer.elapsed());

    }

    {
        youtils::wall_timer timer;

        dStore->sync();

        //        interval.stop();
        LOG_FATAL("Synced " << clusters << " clusters in " <<
                  timer.elapsed());
    }

    if (geteuid() == 0)
    {
        LOG_FATAL("Dropping caches");
        std::ofstream f("/proc/sys/vm/drop_caches");
        f << "1";
        f.close();
    }
    else
    {
        LOG_FATAL("Not dropping caches, run as root!");
    }
    // Redo if this test is reenabled
    // std::vector<ClusterReadDescriptor>
    //     descs(num_clusters,
    //           ClusterReadDescriptor(ClusterLocation(),
    //                                 0,
    //                                 0,
    //                                 0));


    // {
    //     wall_timer interval;
    //     //        interval.start();

    //     clusters = 0;

    //     for (size_t i = 0; i < locv.size(); ++i)
    //     {
    //         ASSERT(locv[i].size() == descs.size());

    //         for (size_t j = 0; j < locv[i].size(); ++j)
    //         {
    //             ClusterAddress ca = (i * num_clusters + j) * vol->getClusterSize();
    //             descs[j] = ClusterReadDescriptor(locv[i][j],
    //                                              ca,
    //                                              &bufv[0],
    //                                              0);
    //         }

    //         dStore->readClusters(&descs[0],
    //                              descs.size());

    //         clusters += descs.size();
    //     }

    //     //        interval.stop();
    //     LOG_FATAL("Read " << clusters << " clusters in " <<
    //               interval.elapsed());
    // }
}
*/
// a simple micro benchmark, generally not of interest for test suite runs
// disabled cause of the missing operator=(ClusterReadDescriptor&...
#if  0
TEST_P(cases, DISABLED_DataStorePerformance)
{
    size_t count = 10;
    char *countEnv = getenv("VD_TEST_DSTORE_IO_ITERATIONS");
    if (countEnv) {
        count = strtoul(countEnv, NULL, 0);
        if (count == ULONG_MAX) {
            FAIL() << "cannot convert " << count << " to unsigned long: " <<
                strerror(errno);
        }
    }

    Volume *vol = newVolume("1volume",
                            backend::Namespace("openvstorage-volumedrivertest-namespace1"));

    SCOPED_BLOCK_BACKEND(vol);

    size_t num_clusters = 8;
    size_t bufSize = num_clusters * vol->getClusterSize();
    std::vector<uint8_t> bufv(bufSize);

    std::vector<std::vector<ClusterLocation> > locv(count / num_clusters);

    for (size_t i = 0; i < count / num_clusters; ++i)
    {
        locv[i].resize(num_clusters);
    }

    DataStoreNG* dStore = vol->getDataStore();

    size_t clusters = 0;
    {
        youtils::wall_timer interval;
        //        interval.start();

        uint32_t throttle;

        for (size_t i = 0; i < count / num_clusters; ++i)
        {

            dStore->writeClusters(&bufv[0],
                                  locv[i],
                                  locv[i].size(),
                                  throttle);
            clusters += locv[i].size();
        }
        //        interval.stop();
        LOG_FATAL("Wrote " << clusters << " clusters in " <<
                  interval.elapsed());

    }

    {
        youtils::wall_timer interval;
        //        interval.start();

        dStore->sync();

        //        interval.stop();
        LOG_FATAL("Synced " << clusters << " clusters in " <<
                  interval.elapsed());
    }

    if (geteuid() == 0)
    {
        LOG_FATAL("Dropping caches");
        std::ofstream f("/proc/sys/vm/drop_caches");
        f << "1";
        f.close();
    }
    else
    {
        LOG_FATAL("Not dropping caches, run as root!");
    }

    {
        std::vector<ClusterReadDescriptor>
            descs(1,
                  ClusterReadDescriptor(ClusterLocationAndHash(),
                                        0,
                                        0,
                                        0));

        clusters = 0;

        youtils::wall_timer interval;
        //  interval.start();

        for (size_t i = 0; i < locv.size(); ++i)
        {
            for (size_t j = 0; j < locv[i].size(); ++j)
            {
                ClusterAddress ca = (i * num_clusters + j) * vol->getClusterSize();
                ClusterLocationAndHash loc_and_hash(locv[i][j], growWeed());

                descs[0] = ClusterReadDescriptor(loc_and_hash,
                                                 ca,
                                                 &bufv[0],
                                                 0);
                dStore->readClusters(descs);
                clusters += descs.size();
            }
        }

        //        interval.stop();
        LOG_FATAL("Read " << clusters << " clusters in " <<
                  interval.elapsed());
    }
}
#endif
TEST_P(cases, DISABLED_weirdSnapshotsFile)
{
    auto ns_ptr = make_random_namespace();
    const std::string volName = "volume";

    Volume* v = newVolume(volName,
                          ns_ptr->ns());

    const std::string pattern1 = "11111111";
    writeToVolume(v,
                  0,
                  v->getClusterSize(),
                  pattern1);

    v->createSnapshot("snap1");

    waitForThisBackendWrite(v);
    waitForThisBackendWrite(v);

    const std::string pattern2 = "22222222";
    writeToVolume(v,
                  0,
                  v->getClusterSize(),
                  pattern2);

    v->createSnapshot("snap2");

    const std::string pattern3 = "33333333";
    writeToVolume(v,
                  0,
                  v->getClusterSize(),
                  pattern3);

    waitForThisBackendWrite(v);
    waitForThisBackendWrite(v);

    // break here ...
    v->restoreSnapshot("snap1");

    checkVolume(v,
                0,
                v->getClusterSize(),
                pattern1);

    // writeToVolume(v,
    //               0,
    //               v->getClusterSize(),
    //               pattern3);

    // v->createSnapshot("snap3");

    // waitForThisBackendWrite(v);
    // waitForThisBackendWrite(v);

    const VolumeConfig cfg(v->get_config());

    // ... and here ...
    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);

    // ... and here ...
    restartVolume(cfg);
    v = getVolume(VolumeId(volName));

    // and here, and look at snapshots.xml in the metadatastore and in the backend
    ASSERT_THROW(v->restoreSnapshot("snap2"),
                 fungi::IOException);
}

TEST_P(cases, DISABLED_boostThreadSleep)
{
    youtils::wall_timer i;


    boost::this_thread::sleep(boost::posix_time::microseconds(4 * 1000));
    LOG_FATAL("slept for " << i.elapsed() << " secs");
}

TEST_P(cases, DISABLED_propertyTree)
{
    using namespace boost::property_tree;
    ptree pt;
    read_xml("testconfig.cfg", pt, xml_parser::trim_whitespace);
    //    write_xml("testconfig.cfg.out", pt, std::locale(), xml_parser::xml_writer_make_settings(' ', 4));
}

// to be run on top of an aptly configured fawltyfs

TEST_P(cases, DISABLED_test_snapshot_persistor)
{
    const auto fname(youtils::System::get_env_with_default<std::string>("SP_TEST_FILE",
                                                                        ""));
    ASSERT_TRUE(fname != "") << "SP_TEST_FILE env var is not set";

    const fs::path p(fname);
    youtils::wall_timer wt;

    SnapshotPersistor sp(p);

    std::cout << "deserialize wall: " << wt.elapsed() << std::endl;

    const fs::path tmp(getTempPath("snaps.xml"));
    ALWAYS_CLEANUP_FILE(tmp);

    wt.restart();

    sp.saveToFile(tmp, SyncAndRename::T);

    std::cout << "serialize wall: " << wt.elapsed() << std::endl;
}

TEST_P(cases, DISABLED_test_snapshot_persistor_deserialize)
{
    const auto fname(youtils::System::get_env_with_default<std::string>("SP_TEST_FILE",
                                                                        ""));
    ASSERT_TRUE(fname != "") << "SP_TEST_FILE env var is not set";

    const fs::path p(fname);
    youtils::wall_timer wt;

    SnapshotPersistor sp(p);

    std::cout << "deserialize wall: " << wt.elapsed() << std::endl;
}

namespace
{
struct Func_
{
    Func_(bool& done, boost::mutex& m, boost::condition_variable& cv)
        : done_(done)
        , m_(m)
        , cv_(cv)
    {}

    void
    operator()()
    {
        boost::lock_guard<boost::mutex> g(m_);
        cv_.notify_one();
        done_ = true;
    }

    bool& done_;
    boost::mutex& m_;
    boost::condition_variable& cv_;
};
}

TEST_P(cases, boost_thread_timedjoin)
{
    bool done = false;
    boost::mutex m;
    boost::unique_lock<boost::mutex> u(m);
    boost::condition_variable cv;

    Func_ f(done, m, cv);
    boost::thread t(boost::ref(f));
    EXPECT_FALSE(t.timed_join(boost::posix_time::milliseconds(10)));

    while (not done)
    {
        cv.wait(u);
    }

    cv.timed_wait(u, boost::posix_time::seconds(10));
    EXPECT_TRUE(t.timed_join(boost::posix_time::milliseconds(10)));
    EXPECT_FALSE(t.timed_join(boost::posix_time::milliseconds(10)));
}

// food for valgrind
TEST_P(cases, DISABLED_leak_some_memory)
{
    uint8_t* leaked = new uint8_t[16];
    memset(leaked, 0x0, 16);
}

TEST_P(cases, DISABLED_uninitialized_memory)
{
    std::unique_ptr<bool> b(new bool);
    if (*b)
    {
        LOG_TRACE("the memory contains something truthy");
    }
    else
    {
        LOG_TRACE("the memory contains something falsey");
    }
}

}

OUR_STRONG_NON_ARITHMETIC_TYPEDEF(boost::filesystem::path, path_type_1, blub);

namespace blub
{

BOOST_STRONG_TYPEDEF(boost::filesystem::path, path_type_2);

static inline std::ostream&
operator<<(std::ostream& os,
           const path_type_2& p)
{
    os << static_cast<const boost::filesystem::path&>(p);
    return os;
}

}

namespace volumedriver
{

TEST_P(cases, DISABLED_print_strong_typedef)
{
    const blub::path_type_1 p("/some/path");
    const blub::path_type_2 q("/some/other/path");

    std::cout << p << ", " << q << std::endl;
    LOG_TRACE(p << ", " << q);
}

// boost::ptree's json_parser puts everything as strings
TEST_P(cases, DISABLED_ptree_numbers)
{
    bpt::ptree pt;
    pt.put("float", 1.333);
    std::stringstream ss;
    bpt::write_json(ss, pt);
    std::cout << ss.str() << std::endl;
}

// trying to understand OVS-73:
TEST_P(cases, DISABLED_program_options)
{
    std::vector<std::string> args;
    args.push_back(std::string("--foo"));

    const std::string default_foo("foo");
    args.push_back(default_foo);

    const std::string default_foobar("FOOBAR");
    std::string foobar;

    po::options_description opts("some options");
    opts.add_options()
        ("foobar",
         po::value<std::string>(&foobar)->default_value(default_foobar),
         "foobar");

    {
        po::command_line_parser p = po::command_line_parser(args);
        p.options(opts);
        p.allow_unregistered();
        po::parsed_options parsed = p.run();
        po::variables_map vm;
        po::store(parsed, vm);
        po::notify(vm);

        EXPECT_EQ(default_foo, foobar);
    }

    {
        po::command_line_parser p = po::command_line_parser(args);
        p.style(po::command_line_style::default_style ^ po::command_line_style::allow_guessing);
        p.options(opts);
        p.allow_unregistered();
        po::parsed_options parsed = p.run();
        po::variables_map vm;
        po::store(parsed, vm);
        po::notify(vm);

        EXPECT_EQ(default_foobar, foobar);
    }
}

// OVS-170: nested keys need to be removed explicitly - simply specifying the path
// is not supported.
TEST_P(cases, DISABLED_erase_from_property_tree)
{
    bpt::ptree pt;
    EXPECT_TRUE(pt.empty());

    const std::string parent("some");
    const std::string child("key");
    const std::string key(parent + "." + child);
    pt.put(key, "some-val");
    EXPECT_FALSE(pt.empty());

    pt.erase(key);
    EXPECT_FALSE(pt.empty());
    EXPECT_TRUE(static_cast<bool>(pt.get_optional<std::string>(key)));

    pt.erase(parent);
    EXPECT_FALSE(pt.get_optional<std::string>(key));
    EXPECT_TRUE(pt.empty());
}

// Another nice one: don't use std::numeric_limits with BOOST_STRONG_TYPEDEF - it'll return
// a max() of '0' for SCOCloneID and SCOVersion (SCONumber and SCOOffset are plain
// typedef's).
// As a bonus, there are actually 2 versions of BOOST_STRONG_TYPEDEF: one in
// boost/strong_typedef.hpp, and one in boost/serialization/strong_typedef.hpp.
// The latter invokes the underlying type's default constructor in the default constructor,
// while the former does not. Together with gcc-4.8.1's numeric_limits implementation
// as outlined below, this makes valgrind unhappy:

// template<typename _Tp>
//   struct numeric_limits : public __numeric_limits_base
//   {
//       // g++-4.6.3 ...
//
//       /** The minimum finite value, or for floating types with
//           denormalization, the minimum positive normalized value.  */
//       static _GLIBCXX_CONSTEXPR _Tp
//       min() throw() { return static_cast<_Tp>(0); }
//
//       /** The maximum finite value.  */
//       static _GLIBCXX_CONSTEXPR _Tp
//       max() throw() { return static_cast<_Tp>(0); }
//
//       // ... vs. g++-4.8.2
//
//       /** The minimum finite value, or for floating types with
//           denormalization, the minimum positive normalized value.  */
//       static _GLIBCXX_CONSTEXPR _Tp
//       min() _GLIBCXX_USE_NOEXCEPT { return _Tp(); }
//
//       /** The maximum finite value.  */
//       static _GLIBCXX_CONSTEXPR _Tp
//       max() _GLIBCXX_USE_NOEXCEPT { return _Tp(); }
//
//       ...
TEST_P(cases, DISABLED_numeric_limits)
{
#define P(t)                                                            \
    "\t" #t << ": " << static_cast<uint64_t>(std::numeric_limits<t>::max()) << std::endl

    std::cout << "std::numeric_limits<T>::max() " << std::endl <<
        P(uint8_t) <<
        P(uint16_t) <<
        P(uint32_t) <<
        P(uint64_t) <<
        P(SCOCloneID) <<
        P(SCOVersion) <<
        P(SCONumber) <<
        P(SCOOffset);

#undef P
}

TEST_P(cases, DISABLED_regex)
{
    const std::string rex("^(?:(?!ab).)+$");
    const std::string match("acb");
    const std::string nomatch("abc");

    boost::regex bregex(rex);

    EXPECT_TRUE(boost::regex_match(match, bregex)) << rex << " should match " << match;
    EXPECT_FALSE(boost::regex_match(nomatch, bregex)) << rex << " should not match " <<
        nomatch;

    std::regex sregex(rex);

    EXPECT_TRUE(std::regex_match(match, sregex)) << rex << " should match " << match;
    EXPECT_FALSE(std::regex_match(nomatch, sregex)) << rex << " should not match " <<
        nomatch;
}

TEST_P(cases, DISABLED_arakoon_user_fun)
{
    const ara::NodeID node_id(yt::System::get_env_with_default("ARAKOON_NODE_ID",
                                                               std::string("Rocky")));
    const std::string addr(yt::System::get_env_with_default("ARAKOON_ADDRESS",
                                                            std::string("127.0.0.1")));
    const uint16_t port = yt::System::get_env_with_default("ARAKOON_PORT",
                                                           13579);
    const ara::ClusterID cluster_id(yt::System::get_env_with_default("ARAKOON_CLUSTER_ID",
                                                                     std::string("Raccoon")));

    const std::vector<ara::ArakoonNodeConfig> cfgs{ ara::ArakoonNodeConfig(node_id,
                                                                           addr,
                                                                           port) };

    ara::Cluster cluster(cluster_id,
                         cfgs);

    cluster.hello("volumedrivertester");

    const std::string sent("An utterly pointless message.");
    const std::string rcvd(cluster.user_function<std::string, std::string>("com.openvstorage.volumedriver.echo",
                                                                           sent));

    EXPECT_EQ(sent, rcvd);
}

namespace
{

struct TestEntry
    : public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>
    , public boost::intrusive::slist_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>
{
    typedef uint64_t key_t;

    TestEntry()
        : key(make_key())
    {}

    const key_t key;

    static uint64_t
    make_key()
    {
        static boost::random::mt19937 rnd;
        static boost::random::uniform_int_distribution<key_t> dist(std::numeric_limits<key_t>::min(),
                                                                   std::numeric_limits<key_t>::max());

        return dist(rnd);
    }
};

}

// SSOBF-10309: Getting cluster cache stats takes looooong if the cache is large and
// filled. The LRU list (intrusive list without a constant time size() call) looks
// like a candidate.
TEST_P(cases, DISABLED_cluster_cache_lru_list_size)
{
    const uint64_t count = yt::System::get_env_with_default("KAK_ENTRIES",
                                                            1ULL << 20);

    typedef boost::intrusive::list<TestEntry,
                                   boost::intrusive::constant_time_size<false> > ListType;

    ListType list;

    std::vector<std::unique_ptr<TestEntry>> ptrs;
    ptrs.reserve(count);

    for (uint64_t i = 0; i < count; ++i)
    {
        ptrs.emplace_back(new TestEntry());
        list.push_back(*ptrs[i]);
    }

    yt::wall_timer t;
    const uint64_t lsize = list.size();
    const double duration = t.elapsed();

    ASSERT_EQ(count, lsize);

    std::cout << "Getting the size of an LRU list with " << lsize << " entries took " <<
        duration << " seconds" << std::endl;
}

// ... with that out of the system, let's see if asking the size from the map is
// any better:
#if 0
TEST_P(cases, DISABLED_cluster_cache_map_size)
{
    const uint64_t count = yt::System::get_env_with_default("KAK_ENTRIES",
                                                            1ULL << 20);
    const uint64_t entries_per_bin = yt::System::get_env_with_default("KAK_ENTRIES_PER_BIN",
                                                                      2);
    typedef ClusterCacheMap<TestEntry> MapType;

    MapType map;
    map.resize(entries_per_bin, count);

    std::vector<std::unique_ptr<TestEntry>> ptrs;
    ptrs.reserve(count);

    for (uint64_t i = 0; i < count; ++i)
    {
        ptrs.emplace_back(new TestEntry());
        map.insert(*ptrs[i]);
    }

    yt::wall_timer t;
    const uint64_t msize = map.entries();
    const double duration = t.elapsed();

    ASSERT_EQ(count, msize);

    std::cout << "Getting the size of a ClusterCacheMap (avg. entries per bin: " <<
        entries_per_bin << ") with " << msize << " entries took " << duration <<
        " seconds" << std::endl;

    std::cout << "Map stats: " << std::endl;

    for (const auto& e : map.stats())
    {
        std::cout << "\t" << e.first << ", " << e.second << std::endl;
    }
}
#endif

// Cf. OVS-1400
// This test is not very interesting really as it doesn't expose a bug but only
// shows that barrier tasks only wait until all previous tasks are done but not
// that subsequent tasks only wait until the barrier task is finished -- you need
// a double barrier for that.
TEST_P(cases, DISABLED_threadpool_barrier)
{
    std::atomic<uint32_t> count(0);
    const size_t iterations = yt::System::get_env_with_default("ITERATIONS",
                                                                 100000ULL);
    const size_t batchsize = yt::System::get_env_with_default("BATCH_SIZE",
                                                              100ULL);

    auto wrns(make_random_namespace());
    Volume* v = newVolume("volume",
                          wrns->ns());

    for (size_t i = 0; i < iterations; ++i)
    {
        for (size_t j = 0; j < batchsize; ++j)
        {
            auto t(new backend_task::FunTask(*v,
                                        [&count, i]
                                        {
                                            EXPECT_EQ(i, count.load());
                                        }));

            VolManager::get()->scheduleTask(t);
        }

        auto t(new backend_task::FunTask(*v,
                                    [&count]
                                    {
                                        ++count;
                                    },
                                    yt::BarrierTask::T));

        VolManager::get()->scheduleTask(t);
    }

    waitForThisBackendWrite(v);
}

TEST_P(cases, DISABLED_float_lexical_cast)
{
    const float f = 3.147;
    auto s(boost::lexical_cast<std::string>(f));
    const float g = boost::lexical_cast<float>(s);

    EXPECT_EQ(f, g);
}

}

STRONG_TYPED_STRING(volumedrivertest, CasesTestId);

namespace volumedriver
{

TEST_P(cases, DISABLED_optional_streaming)
{
    const boost::optional<volumedrivertest::CasesTestId> none;
    std::cout << "None: " << none << std::endl;

    const boost::optional<volumedrivertest::CasesTestId> some("RandomIdentifier");
    std::cout << "Some: " << some << std::endl;
}

INSTANTIATE_TEST(cases);

}

// Local Variables: **
// mode: c++ **
// End: **
