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

#include "../CachedMetaDataStore.h"
#include "../TokyoCabinetMetaDataBackend.h"
#include "../MetaDataStoreBuilder.h"

#include <boost/filesystem.hpp>

namespace volumedrivertest
{

namespace be = backend;
namespace fs = boost::filesystem;
namespace vd = volumedriver;
namespace vdt = volumedrivertest;
namespace yt = youtils;

using namespace std::string_literals;
using namespace volumedriver;

class MetaDataStoreBuilderTest
    : public vd::VolManagerTestSetup
{
public:
    MetaDataStoreBuilderTest()
        : vd::VolManagerTestSetup("MetaDataStoreBuilderTest")
    {}

protected:
    class MDStoreComparator
        : public vd::MetaDataStoreFunctor
    {
    public:
        MDStoreComparator(vd::MetaDataStoreInterface& orig)
            : vd::MetaDataStoreFunctor()
            , orig_(orig)
        {}

        virtual ~MDStoreComparator() = default;

        virtual void
        operator()(vd::ClusterAddress ca,
                   const vd::ClusterLocationAndHash& clh)
        {
            vd::ClusterLocationAndHash exp;
            orig_.readCluster(ca,
                              exp);

            EXPECT_EQ(exp, clh) << "mismatch at CA " << ca;
        }

    private:
        vd::MetaDataStoreInterface& orig_;
    };

    virtual void
    check(vd::Volume& vol,
          vd::MetaDataStoreInterface& copy)
    {
        const fs::path scratch_dir(directory_ / "scratch");
        fs::create_directories(scratch_dir);

        be::BackendInterfacePtr bi(vol.getBackendInterface()->clone());

        vd::MetaDataStoreBuilder(copy,
                                 bi->clone(),
                                 scratch_dir)();

        const vd::ClusterAddress max_ca =
            vol.getSize() / vol.getClusterSize();

        vd::MetaDataStoreInterface& orig(*vol.getMetaDataStore());

        EXPECT_EQ(copy.lastCork(),
                  orig.lastCork());

        MDStoreComparator cmp(copy);

        orig.for_each(cmp,
                      max_ca);
    }
};

TEST_P(MetaDataStoreBuilderTest, basics)
{
    auto ns(make_random_namespace());
    vd::Volume* v = newVolume(*ns,
                              vd::VolumeSize(4ULL << 20));

    const fs::path db_dir(directory_ / "db_copy");
    fs::create_directories(db_dir);

    auto tc(std::make_shared<vd::TokyoCabinetMetaDataBackend>(db_dir,
                                                              true));

    be::BackendInterfacePtr bi(v->getBackendInterface()->clone());

    vd::CachedMetaDataStore copy(tc,
                                 "copy-of-"s + bi->getNS().str());

    check(*v,
          copy);

    const std::string pattern1("before-first-snapshot");
    writeToVolume(v,
                  0,
                  v->getSize() / 2,
                  pattern1);

    const std::string snap1("snap1");
    v->createSnapshot(snap1);
    waitForThisBackendWrite(v);

    check(*v,
          copy);

    check(*v,
          copy);

    const std::string pattern2("before-second-snapshot");
    writeToVolume(v,
                  0,
                  v->getSize() / 4,
                  pattern2);

    const std::string snap2("snap2");
    v->createSnapshot(snap2);
    waitForThisBackendWrite(v);

    check(*v,
          copy);

    restoreSnapshot(v, snap1);

    check(*v,
          copy);
}

TEST_P(MetaDataStoreBuilderTest, clone)
{
    auto ns(make_random_namespace());
    vd::Volume* v = newVolume(*ns,
                              vd::VolumeSize(4ULL << 20));

    const std::string pattern1("parent");
    writeToVolume(v,
                  0,
                  v->getSize() / 2,
                  pattern1);

    const std::string psnap("psnap");
    v->createSnapshot(psnap);
    waitForThisBackendWrite(v);

    auto clone_ns(make_random_namespace());
    vd::Volume* c = createClone(*clone_ns,
                                ns->ns(),
                                psnap);

    const std::string pattern2("clone");

    writeToVolume(v,
                  0,
                  v->getSize() / 4,
                  pattern2);

    const std::string csnap("csnap");
    c->createSnapshot(csnap);
    waitForThisBackendWrite(c);

    be::BackendInterfacePtr bi(c->getBackendInterface()->clone());

    const fs::path db_dir(directory_ / "db_copy");
    fs::create_directories(db_dir);

    auto tc(std::make_shared<vd::TokyoCabinetMetaDataBackend>(db_dir,
                                                              true));

    vd::CachedMetaDataStore copy(tc,
                                 "copy-of-"s + bi->getNS().str());

    check(*c,
          copy);
}

INSTANTIATE_TEST(MetaDataStoreBuilderTest);

}
