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
#include "VolManagerTestSetup.h"

#include <cassert>
#include <map>
#include <iostream>
#include <sstream>
#include <string>

#include <youtils/wall_timer.h>
#include <youtils/Weed.h>

#include <../CachedMetaDataStore.h>
#include <../MetaDataStoreInterface.h>
#include <../TokyoCabinetMetaDataBackend.h>
#include <../Volume.h>

namespace volumedrivertest
{

using namespace volumedriver;
namespace yt = youtils;

class TokyoCabinetMetaDataStoreTest
    : public VolManagerTestSetup
{
public:
    TokyoCabinetMetaDataStoreTest()
        : VolManagerTestSetup("TokyoCabinetMetaDataStoreTest")
    {}

    uint64_t
    tc_db_size_estimate(const Volume& v)
    {
        return TokyoCabinetMetaDataBackend::locally_required_bytes_(v.get_config());
    }
};

// reanimate this one one of these days, or just throw it away?
#if 0
TEST_P(TokyoCabinetMetaDataStoreTest, DISABLED_growth_of_tc_file)
{
    if(VolManager::get()->useArakoonMetaDataStore())
    {
        return;
    }

    Volume* vol_ = newVolume("volume1",
                             "openvstorage-volumedrivertest-namespace1");

    MetaDataStoreInterface* mdi = vol_->getMetaDataStore();

    TCBTMetaDataStore* md = dynamic_cast<TCBTMetaDataStore*>(mdi);
    ASSERT_TRUE(md);

    ASSERT_NO_THROW(mdi->sync());

    fs::path metadata_path = VolManager::get()->getMetaDataPath(vol_) / "mdstore.tc";
    ASSERT_TRUE(fs::exists(metadata_path));
    ASSERT_TRUE(fs::is_regular_file(metadata_path));
    int num_test = 100;
    std::vector<uint64_t> file_sizes;
    uint64_t page_size = 1 << 11;

    file_sizes.reserve(page_size*num_test);
    file_sizes.push_back(fs::file_size(metadata_path));
    ClusterLocationAndHash loc_and_hash;
    uint64_t num_writes = 0;

    for(uint offset = 0; offset < page_size; ++offset)
    {

        for(int i = 0; i < num_test; ++i)
        {
            ClusterAddress ca = (i  << 11) + offset;
            mdi->writeCluster(ca,
                             loc_and_hash);

            mdi->sync();

            file_sizes.push_back(fs::file_size(metadata_path));

            int64_t cur_diff = std::max(file_sizes[num_writes+1],file_sizes[num_writes])
                - std::min(file_sizes[num_writes+1], file_sizes[num_writes]);
            EXPECT_TRUE(file_sizes[num_writes+1] >= file_sizes[num_writes]) << "Shrinking by " << cur_diff;
            ++num_writes;
            EXPECT_TRUE(offset == 0
                        or cur_diff == 0) << "diff:" << cur_diff << ", entry: " << i <<  ", offset: "  << offset << std::endl;
        }

        if(offset == 0)
        {
            std::cerr << "size after first round of writes " << file_sizes.back() << std::endl;
        }


        EXPECT_EQ(md->size_in_pages(), num_test);
    }


    std::cerr << "expected md store without overwrite " << (num_writes * 40000) << std::endl;
    std::cerr << "expected md store with overwrite " << (num_test * 40000) << std::endl;
    std::cerr << "mdstore file size before optimize " << file_sizes.back() << std::endl;
    md->optimize();
    file_sizes.push_back(fs::file_size(metadata_path));
    std::cerr << "mdstore file size after optimize " << file_sizes.back() << std::endl;
}
#endif

TEST_P(TokyoCabinetMetaDataStoreTest, DISABLED_on_disk_size_estimate)
{
    if (metadata_backend_type() == MetaDataBackendType::Arakoon)
    {
        return;
    }

    const char* max_vsize_str = getenv("VD_TEST_MDSTORE_MAX_SIZE");
    const uint64_t max_vsize = max_vsize_str ?
        boost::lexical_cast<uint64_t>(max_vsize_str) : 2ULL << 40;
    const uint64_t min_vsize = 16ULL << 20;

    std::stringstream ss;

    for (uint64_t vsize = max_vsize; vsize >= min_vsize; vsize >>= 1)
    {
        if (vsize != max_vsize)
        {
            ss << ", ";
        }

        Volume* v = nullptr;

        ASSERT_NO_THROW(v = newVolume("volume",
                                      backend::Namespace(),
                                      VolumeSize(vsize)));

        const auto& mdi = v->getMetaDataStore();
        auto md = dynamic_cast<CachedMetaDataStore*>(mdi);
        ASSERT_TRUE(md);

        const uint64_t pentries = CachePage::capacity() / sizeof(ClusterLocationAndHash);
        for (uint64_t j = 0; j < vsize / v->getClusterSize(); j += pentries)
        {
            ClusterLocation loc(j + 1);
            youtils::Weed w = growWeed();
            const ClusterLocationAndHash clh(loc, w);
            md->writeCluster(j, clh);
        }

        md->unCork(boost::none);
        syncMetaDataStoreBackend(v);

        const uint64_t real = TokyoCabinetMetaDataBackend::locally_used_bytes(v->get_config());
        const uint64_t expected = tc_db_size_estimate(*v);
        const uint64_t epsilon = real * 0.005;

        ss << "(" << vsize << ", " << real << ")";

        EXPECT_GE(expected, real - epsilon) << "volume size " << v->getSize();
        EXPECT_LE(expected, real + epsilon) << "volume size " << v->getSize();

        destroyVolume(v,
                      DeleteLocalData::T,
                      RemoveVolumeCompletely::T);
    }

    std::cout << "[vsize, mdstore size]:" << std::endl;
    std::cout << ss.str() << std::endl;
}

INSTANTIATE_TEST(TokyoCabinetMetaDataStoreTest);

}

// Local Variables: **
// mode: c++ **
// End: **
