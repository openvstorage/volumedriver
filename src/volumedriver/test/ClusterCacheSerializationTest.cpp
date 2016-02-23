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

#include "../ClusterCacheDevice.h"
#include "../ClusterCache.h"

#include <boost/filesystem/fstream.hpp>

#include <youtils/DimensionedValue.h>
#include <youtils/wall_timer.h>
#include <youtils/cpu_timer.h>
#include <youtils/SourceOfUncertainty.h>

namespace volumedrivertest
{

using namespace volumedriver;

namespace bpt = boost::property_tree;
namespace yt = youtils;
using namespace initialized_params;

class ClusterCacheFakeStore // Throws away your data, oh yeah!
{
public:
    static size_t
    default_cluster_size()
    {
        return 4096;
    }

    ClusterCacheFakeStore()
        : total_size_(0)
    {}

    ClusterCacheFakeStore(const fs::path& path,
                          const uint64_t size,
                          const size_t csize)
        : path_(path)
        , cluster_size_(csize)
        , total_size_(size - (size % cluster_size_))
     {}

    ClusterCacheFakeStore(const ClusterCacheFakeStore&) = delete;
    ClusterCacheFakeStore& operator=(const ClusterCacheFakeStore&) = delete;

    void
    write_guid(const UUID& uuid)
    {
        uuid_ = uuid;
    }

    bool
    check_guid(const UUID& uuid)
    {
        return uuid_ == uuid;
    }

    ssize_t
    read(uint8_t* /*buf*/,
         uint32_t /*offset*/)
    {
        return cluster_size_;
    }

    void
    check(const yt::Weed&,
         uint32_t)
    {}


    ssize_t
    write(const uint8_t* /*buf*/,
          uint32_t /*entry*/)
    {
        return cluster_size_;
    }


    uint64_t
    total_size() const
    {
        return total_size_;
    }

    const fs::path&
    path() const
    {
        return path_;
    }

    void
    reinstate()
    {}

    void
    sync()
    {}

    size_t
    cluster_size() const
    {
        return cluster_size_;
    }

private:
    friend class boost::serialization::access;

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        std::string str;

        ar & str;
        ar & total_size_;
        ar & uuid_;

        ASSERT_LT(0, version);

        ar & cluster_size_;

        ASSERT_LT(0,
                  cluster_size_);

        const_cast<fs::path&>(path_) = str;
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned /* version */) const
    {
        ASSERT_LT(0,
                  cluster_size_);

        std::string str = path_.string();
        ar & str;
        ar & total_size_;
        ar & uuid_;
        ar & cluster_size_;
    }

    const fs::path path_;
    // what is called total_size is in fact clustersize smaller than available.
    size_t cluster_size_;
    uint64_t total_size_;
    UUID uuid_;
};


typedef ClusterCacheDeviceT<ClusterCacheFakeStore> FakeDevice;

TODO("AR: more descriptive test names");

class ClusterCacheSerializationTest
    : public VolManagerTestSetup
{
protected:
    typedef ClusterCacheT<FakeDevice> ClusterCacheType;
    typedef ClusterCacheType::ManagerType ManagerType;

    ClusterCacheSerializationTest()
        : VolManagerTestSetup("ClusterCacheSerializationTest")
        , serial_dir(directory_ / "serialization")
    {}

    void
    SetUp()
    {
        VolManagerTestSetup::SetUp();
        fs::create_directories(serial_dir);
    }

    fs::path
    SetupDevice(const std::string& name,
                yt::DimensionedValue& size,
                std::vector<MountPointConfig>& vec)
    {
        fs::path device = directory_ / name;
        int fd = -1;
        fd = open(device.string().c_str(),
                  O_RDWR| O_CREAT ,
                  S_IWUSR|S_IRUSR);
        if(fd < 0)
        {
            LOG_ERROR("Couldn't open file " << tmpfile << ", " << strerror(errno));
            throw fungi::IOException("Couldn't open file");
        }
        if((errno=posix_fallocate(fd, 0,size.getBytes()))!=0)
        {
            LOG_ERROR("Could not allocate file " << tmpfile << " with size " << size.getBytes() << ", " << strerror(errno));
            throw fungi::IOException("Could not allocate file ");
        }
        fchmod(fd,00600);

        close(fd);

        vec.push_back(MountPointConfig(device, size.getBytes()));

        return device;
    }

    template<typename T>
    std::vector<MountPointConfig>
    make_mountpoint_configs(const T& sizes)
    {
        std::vector<MountPointConfig> vec;

        for (const auto& val : sizes)
        {
            vec.push_back(MountPointConfig(val.first, val.second));
        }

        return vec;
    }

    void
    fillConfigurationPropertyTree(bpt::ptree& pt,
                                  uint64_t average_entries_per_bin,
                                  const std::vector<MountPointConfig>& vec,
                                  bool serialize = true)
    {
        PARAMETER_TYPE(serialize_read_cache)(serialize).persist(pt);
        PARAMETER_TYPE(read_cache_serialization_path)(serial_dir.string()).persist(pt);
        PARAMETER_TYPE(average_entries_per_bin)(average_entries_per_bin).persist(pt);
        PARAMETER_TYPE(clustercache_mount_points)(vec).persist(pt);

        pt.put("version", 1);
    }

    template<typename T>
    void
    fillConfigurationPropertyTree(bpt::ptree& pt,
                                  uint64_t average_entries_per_bin,
                                  const T& sizes,
                                  bool serialize = true)
    {
        fillConfigurationPropertyTree(pt,
                                      average_entries_per_bin,
                                      make_mountpoint_configs(sizes),
                                      serialize);
    }

    ClusterCacheKey
    random_key(const ClusterCacheHandle handle)
    {
        if (handle != ClusterCacheHandle(0))
        {
            return ClusterCacheKey(handle,
                                   sou(std::numeric_limits<uint64_t>::max()));
        }
        else
        {
            return ClusterCacheKey(ClusterCacheHandle(sou(std::numeric_limits<uint64_t>::max())),
                                   sou(1UL,
                                       std::numeric_limits<uint64_t>::max()));
        }
    }

    void
    test_device_serialization_0(ClusterCacheMode mode)
    {
        std::vector<uint8_t> buf(4096);
        fs::path output = FileUtils::create_temp_file_in_temp_dir("ClusterCacheSerializationTest");
        ALWAYS_CLEANUP_FILE(output);
        bpt::ptree pt;

        uint64_t size = sou(1, 10);
        std::stringstream ss;

        ss << ( size << 2) << "KiB";
        yt::DimensionedValue v(ss.str());
        std::list<std::pair<std::string, uint64_t> > sizes;
        sizes.push_back(std::make_pair(UUID().str(), v.getBytes()));

        // ASSERT_TRUE(readCache.maybeAddDevice("no_path",
        //                                      v.getBytes()));
        fillConfigurationPropertyTree(pt,
                                      2,
                                      sizes);

        ClusterCacheType readCache(pt);
        ASSERT_TRUE(readCache.totalSizeInEntries() == size);

        const OwnerTag otag(1);
        const ClusterCacheHandle
            handle(readCache.registerVolume(otag,
                                            mode));

        std::vector<std::unique_ptr<ClusterCacheKey>> keys;
        keys.resize(size);

        for(unsigned i = 0; i < size; ++i)
        {
            auto k(std::make_unique<ClusterCacheKey>(random_key(handle)));
            if (mode == ClusterCacheMode::LocationBased)
            {
                ASSERT_EQ(handle,
                          k->cluster_cache_handle());
            }

            readCache.add(handle,
                          *k,
                          buf.data(),
                          buf.size());

            keys[i] = std::move(k);
        }

        for (unsigned i = 0; i < size; ++i)
        {
            if (mode == ClusterCacheMode::LocationBased)
            {
                ASSERT_EQ(handle,
                          keys[i]->cluster_cache_handle());
            }

            ASSERT_TRUE(readCache.read(handle,
                                       *keys[i],
                                       buf.data(),
                                       buf.size()));
        }

        for(unsigned i = 0; i < size; ++i)
        {
            readCache.add(handle,
                          random_key(handle),
                          buf.data(),
                          buf.size());
        }

        for (unsigned i = 0; i < size; ++i)
        {
            if (mode == ClusterCacheMode::LocationBased)
            {
                ASSERT_EQ(handle,
                          keys[i]->cluster_cache_handle());
            }

            ASSERT_FALSE(readCache.read(handle,
                                        *keys[i],
                                        buf.data(),
                                        buf.size()));
        }
    }

    std::unique_ptr<ClusterCacheType>
    make_cache(size_t nentries)
    {
        const size_t mpsize = nentries * 4096;
        std::list<std::pair<std::string, uint64_t>> mps;
        mps.push_back(std::make_pair("no_path_0",
                                     mpsize));
        bpt::ptree pt;
        fillConfigurationPropertyTree(pt,
                                      2,
                                      mps);

        auto cache(std::make_unique<ClusterCacheType>(pt));
        EXPECT_EQ(nentries,
                  cache->totalSizeInEntries());

        return cache;
    }

    void
    test_tiny(ClusterCacheMode mode)
    {
        const size_t mpentries = 1;
        const size_t mpsize = mpentries * 4096;
        std::list<std::pair<std::string, uint64_t>> mps;
        mps.push_back(std::make_pair("no_path_0",
                                     mpsize));
        bpt::ptree pt;
        fillConfigurationPropertyTree(pt,
                                      2,
                                      mps);

        std::unique_ptr<ClusterCacheType> cache(make_cache(1));

        const OwnerTag otag(1);
        const ClusterCacheHandle handle(cache->registerVolume(otag,
                                                              mode));

        std::vector<byte> buf(4096,
                              13);

        const ClusterCacheKey k(random_key(handle));
        cache->add(handle,
                   k,
                   buf.data(),
                   buf.size());

        EXPECT_TRUE(cache->read(handle,
                                k,
                                buf.data(),
                                buf.size()));

        const ClusterCacheKey l(random_key(handle));

        ASSERT_TRUE(k != l);

        cache->add(handle,
                   l,
                   buf.data(),
                   buf.size());

        EXPECT_FALSE(cache->read(handle,
                                 k,
                                 buf.data(),
                                 buf.size()));

        EXPECT_TRUE(cache->read(handle,
                                l,
                                buf.data(),
                                buf.size()));
    }

    void
    test_device_serialization_1(ClusterCacheMode mode)
    {
        bpt::ptree pt;
        uint64_t num_devices = sou(1, 5);
        uint64_t total = 0;
        std::list<std::pair<std::string, uint64_t> > sizes;

        for(unsigned i = 0; i < num_devices; ++i)
        {
            std::stringstream ss1, ss2;
            ss1 << "no_path_" << i;

            ss2 << sou(1, 10) << "GiB";
            yt::DimensionedValue v(ss2.str());
            total += v.getBytes();
            sizes.push_back(std::make_pair(UUID().str(), v.getBytes()));

            // ASSERT_TRUE(readCache.maybeAddDevice(ss1.str(),
            //                                      v.getBytes()));
        }

        const uint64_t read_cache_average_entries = sou(1, 5);

        fillConfigurationPropertyTree(pt,
                                      read_cache_average_entries,
                                      sizes);

        ClusterCacheType readCache(pt);

        std::vector<uint8_t> buf(4096);
        uint64_t num_hits = 0;
        uint64_t num_misses = 0;
        uint64_t num_entries = 0;

        const uint64_t read_cache_constant = sou(2, 5) * (total >> 12);
        const uint64_t size_in_entries = readCache.totalSizeInEntries();

        std::cout << "testing with " << std::endl
                  << "\t" << num_devices << " readcache devices of variable size" << std::endl
                  << "\t" << total << " total size" << std::endl
                  << "\t " << size_in_entries << " number of entries" << std::endl
                  << "writing " << read_cache_constant << " entries " << std::endl;

        readCache.get_stats(num_hits,
                            num_misses,
                            num_entries);
        ASSERT_TRUE(num_hits == 0);
        ASSERT_TRUE(num_misses == 0);
        ASSERT_TRUE(num_entries == 0);

        std::vector<std::unique_ptr<ClusterCacheKey>> keys;
        keys.resize(size_in_entries);

        const OwnerTag otag(1);
        const ClusterCacheHandle handle(readCache.registerVolume(otag,
                                                                 mode));

        for(uint64_t i = 0; i < read_cache_constant; ++i)
        {
            auto k(std::make_unique<ClusterCacheKey>(random_key(handle)));
            if (mode == ClusterCacheMode::LocationBased)
            {
                ASSERT_EQ(handle,
                          k->cluster_cache_handle());
            }

            readCache.add(handle,
                          *k,
                          buf.data(),
                          buf.size());
            keys[i%size_in_entries] = std::move(k);

            if(i%size_in_entries == size_in_entries - 1)
            {
                for(uint64_t j = 0; j < size_in_entries; ++j)
                {
                    ASSERT_TRUE(keys[j] != nullptr);

                    if (mode == ClusterCacheMode::LocationBased)
                    {
                        ASSERT_EQ(handle,
                                  keys[j]->cluster_cache_handle());
                    }

                    ASSERT_TRUE(readCache.read(handle,
                                               *keys[j],
                                               buf.data(),
                                               buf.size()));
                    keys[j] = nullptr;
                }
            }
        }

        for(uint64_t j = 0; j < size_in_entries; ++j)
        {
            if(keys[j])
            {
                if (mode == ClusterCacheMode::LocationBased)
                {
                    ASSERT_EQ(handle,
                              keys[j]->cluster_cache_handle());
                }

                ASSERT_TRUE(readCache.read(handle,
                                           *keys[j],
                                           buf.data(),
                                           buf.size()));
                keys[j] = nullptr;
            }
        }

    }

    void
    make_ze_big_one(ClusterCacheMode mode)
    {
        fs::path output = FileUtils::create_temp_file_in_temp_dir("KAKSerialize");
        ALWAYS_CLEANUP_FILE(output);

        std::vector<uint8_t> buf(4096);
        yt::DimensionedValue v1("5GiB");
        ASSERT_TRUE(v1.getBytes() > (1 << 22));

        const uint64_t read_cache_constant = v1.getBytes() >> 12;
        std::list<std::pair<std::string, uint64_t> > sizes;
        sizes.push_back(std::make_pair(UUID().str(), v1.getBytes()));

        bpt::ptree pt;

        fillConfigurationPropertyTree(pt,
                                      2,
                                      sizes,
                                      false);
        ClusterCacheType readCache(pt);

        const OwnerTag otag(1);
        const ClusterCacheHandle
            handle(readCache.registerVolume(otag,
                                            mode));

        for(uint64_t i = 0; i < read_cache_constant; ++i)
        {
            readCache.add(handle,
                          random_key(handle),
                          buf.data(),
                          buf.size());
        }

        Serialization::serializeAndFlush<ClusterCacheType::oarchive_type>(output, readCache);

        {
            bpt::ptree pt;

            std::list<std::pair<std::string, uint64_t> > sizes;

            fillConfigurationPropertyTree(pt,
                                          2,
                                          sizes,
                                          false);
            ClusterCacheType readCache(pt);

            fs::ifstream ifs(output);
            ClusterCacheType::iarchive_type ia(ifs);
            ia >> readCache;
            ifs.close();
        }
    }

    void
    ze_big_one(ClusterCacheMode mode)
    {
        const int64_t max_test = 32;

        for(int f = 1; f <= max_test ; f *= 2)
        {
            std::cout << "<<<<<< test with " << f << " GiB >>>>>>>>" << std::endl;

            std::cout << sizeof(std::list<uint64_t>) << std::endl;

            fs::path output = FileUtils::create_temp_file_in_temp_dir("ClusterCacheSerializationTest");
            ALWAYS_CLEANUP_FILE(output);

            std::stringstream ss;
            // ss << r.IntegerC(10,50) << "GiB";
            ss << f << "GiB";

            yt::DimensionedValue v1(ss.str());
            ASSERT_TRUE(v1.getBytes() > (1 << 22));

            const uint64_t read_cache_constant = v1.getBytes() >> 12;
            const uint64_t vec_size = read_cache_constant >> 10;
            std::vector<std::unique_ptr<ClusterCacheKey>> vec;
            vec.reserve(vec_size);

            yt::cpu_timer ct;
            yt::wall_timer wt;

            std::vector<uint8_t> buf(4096);

            const OwnerTag otag(1);

            {
                std::list<std::pair<std::string, uint64_t> > sizes;
                sizes.push_back(std::make_pair(UUID().str(), v1.getBytes()));

                bpt::ptree pt;
                fillConfigurationPropertyTree(pt,
                                              2,
                                              sizes,
                                              false);
                ClusterCacheType readCache(pt);

                const ClusterCacheHandle handle(readCache.registerVolume(otag,
                                                                         mode));

                std::cout << "Checking the keys" << std::endl;
                wt.restart();
                ct.restart();

                for(uint64_t i = 0; i < read_cache_constant; ++i)
                {
                    auto k(std::make_unique<ClusterCacheKey>(random_key(handle)));
                    readCache.add(handle,
                                  *k,
                                  buf.data(),
                                  buf.size());

                    if(vec.size() < vec_size and
                       sou(1023) == 0)
                    {
                        vec.emplace_back(std::move(k));
                    }
                }

                std::cout << "created " << read_cache_constant << " keys in "
                          << ct.elapsed() << "seconds cpu time"
                          << wt.elapsed() << "seconds wall time" << std::endl;

                std::cout << "Checking " << vec.size() << " keys" << std::endl;

                for(uint64_t i = 0; i < vec.size(); ++i)
                {
                    ASSERT_TRUE(readCache.read(handle,
                                               *vec[i],
                                               buf.data(),
                                               buf.size()));
                }

                wt.restart();
                ct.restart();

                std::cout << "Streaming out" << std::endl;

                Serialization::serializeAndFlush<ClusterCacheType::oarchive_type>(output,
                                                                                  readCache);

                std::cout << "streamed out " << read_cache_constant << " keys in "
                          << ct.elapsed() << "seconds cpu time"
                          << wt.elapsed() << "seconds wall time" << std::endl;

            }
            // size of file 716800K
            {
                std::cout << "streaming in " << std::endl;
                bpt::ptree pt;
                std::list<std::pair<std::string, uint64_t> > sizes;

                fillConfigurationPropertyTree(pt,
                                              1,
                                              sizes,
                                              false);
                ClusterCacheType readCache(pt);

                fs::ifstream ifs(output);
                wt.restart();
                ct.restart();

                ClusterCacheType::iarchive_type ia(ifs);
                ia >> readCache;
                ifs.close();
                //   readCache.finishInitialization(2);
                std::cout << "streamed in " << read_cache_constant << " keys in "
                          << ct.elapsed() << "seconds cpu time"
                          << wt.elapsed() << "seconds wall time" << std::endl;

                const ClusterCacheHandle handle(readCache.registerVolume(otag,
                                                                         mode));

                std::cout << "Checking keys again: " << std::endl;
                for(uint64_t i = 0; i < vec.size(); ++i)
                {
                    ASSERT_TRUE(readCache.read(handle,
                                               *vec[i],
                                               buf.data(),
                                               buf.size()));
                }
            }
        }

    }

    void
    test_equal_distribution_of_keys(ClusterCacheMode mode)
    {
        bpt::ptree pt;
        std::list<std::pair<std::string, uint64_t> > sizes;

        std::string five_path = UUID().str();
        sizes.push_back(std::make_pair(five_path,5*4096));
        std::string seven_path = UUID().str();

        sizes.push_back(std::make_pair(seven_path,7*4096));
        std::string three_path = UUID().str();

        sizes.push_back(std::make_pair(three_path,3*4096));

        fillConfigurationPropertyTree(pt,
                                      2,
                                      sizes);
        ClusterCacheType read_cache(pt);

        auto check([&](size_t exp_used_clusters_1,
                       size_t exp_used_clusters_2,
                       size_t exp_used_clusters_3)
                   {
                       ClusterCacheType::ManagerType::Info device_info;
                       read_cache.deviceInfo(device_info);
                       EXPECT_EQ(3U, device_info.size());

                       for (const auto& val : device_info)
                       {
                           if(val.first.string() == three_path)
                           {
                               EXPECT_EQ(3U * 4096,
                                         val.second.total_size);
                               EXPECT_EQ(exp_used_clusters_1 * 4096,
                                         val.second.used_size);
                           }
                           else if(val.first.string() == five_path)
                           {
                               EXPECT_EQ(5U * 4096,
                                         val.second.total_size);
                               EXPECT_EQ(exp_used_clusters_2 * 4096,
                                         val.second.used_size);
                           }
                           else if(val.first.string() == seven_path)
                           {
                               EXPECT_EQ(7U * 4096,
                                         val.second.total_size);
                               EXPECT_EQ(exp_used_clusters_3 * 4096,
                                         val.second.used_size);
                           }
                           else
                           {
                               EXPECT_EQ(1,2) << "path unknown";
                           }
                       }
                   });

        check(0, 0, 0);

        const OwnerTag otag(1);
        const ClusterCacheHandle handle(read_cache.registerVolume(otag,
                                                                  mode));

        ASSERT_TRUE(read_cache.totalSizeInEntries() == 15);

        auto write([&](size_t n)
                   {
                       const std::vector<uint8_t> buf(4096);
                       for(unsigned i = 0; i < n; ++i)
                       {
                           read_cache.add(handle,
                                          random_key(handle),
                                          buf.data(),
                                          buf.size());
                       }
                   });

        write(3);
        check(1, 1, 1);

        write(6);
        check(3, 3, 3);

        write(4);
        check(3, 5, 5);

        write(2);
        check(3, 5, 7);

        write(10);
        check(3, 5, 7);
    }


    yt::SourceOfUncertainty sou;
    fs::path serial_dir;
    const static uint32_t current_version = 1;

private:

    DECLARE_LOGGER("ClusterCacheTest");
};

TEST_P(ClusterCacheSerializationTest, DoubleMountPoints)
{
    std::vector<MountPointConfig> vec;
    yt::DimensionedValue d1("1GiB");

    const fs::path p1 = SetupDevice("dev1",
                                    d1,
                                    vec);
    const fs::path p2 = SetupDevice("dev2",
                                    d1,
                                    vec);
    bpt::ptree pt;
    fillConfigurationPropertyTree(pt,
                                  2,
                                  vec);
    {
        ClusterCache read_cache(pt);
        ClusterCache::ManagerType::Info info;
        read_cache.deviceInfo(info);
        EXPECT_EQ(2U, info.size());
    }

    {
        fs::remove(p2);
        fs::create_symlink(p1,p2);
        ClusterCache read_cache(pt);
        ClusterCache::ManagerType::Info info;
        read_cache.deviceInfo(info);
        EXPECT_EQ(1U, info.size());
        UpdateReport rep;

        read_cache.update(pt,
                          rep);

        read_cache.deviceInfo(info);
        EXPECT_EQ(1U, info.size());

        fs::remove(p2);
        SetupDevice("dev2",
                    d1,
                    vec);
        read_cache.update(pt, rep);
        read_cache.deviceInfo(info);
        EXPECT_EQ(2U, info.size());
    }
}

TEST_P(ClusterCacheSerializationTest, tiny_content_based)
{
    test_tiny(ClusterCacheMode::ContentBased);
}

TEST_P(ClusterCacheSerializationTest, tiny_location_based)
{
    test_tiny(ClusterCacheMode::LocationBased);
}

TEST_P(ClusterCacheSerializationTest, equal_distribution_of_keys_content_based)
{
    test_equal_distribution_of_keys(ClusterCacheMode::ContentBased);
}

TEST_P(ClusterCacheSerializationTest, equal_distribution_of_keys_location_based)
{
    test_equal_distribution_of_keys(ClusterCacheMode::LocationBased);
}

TEST_P(ClusterCacheSerializationTest, device_serialization_0_content_based)
{
    test_device_serialization_0(ClusterCacheMode::ContentBased);
}

TEST_P(ClusterCacheSerializationTest, device_serialization_0_location_based)
{
    test_device_serialization_0(ClusterCacheMode::LocationBased);
}

TEST_P(ClusterCacheSerializationTest, device_serialization_0_mixed)
{
    std::vector<uint8_t> buf(4096);
    fs::path output = FileUtils::create_temp_file_in_temp_dir("ClusterCacheSerializationTest");
    ALWAYS_CLEANUP_FILE(output);
    bpt::ptree pt;

    uint64_t size = sou(1, 10);
    std::stringstream ss;

    ss << ( size << 2) << "KiB";
    yt::DimensionedValue v(ss.str());
    std::list<std::pair<std::string, uint64_t> > sizes;
    sizes.push_back(std::make_pair(UUID().str(), v.getBytes()));

    // ASSERT_TRUE(readCache.maybeAddDevice("no_path",
    //                                      v.getBytes()));
    fillConfigurationPropertyTree(pt,
                                  2,
                                  sizes);

    ClusterCacheType readCache(pt);
    ASSERT_TRUE(readCache.totalSizeInEntries() == size);

    const OwnerTag ltag(1);
    const ClusterCacheHandle
        lhandle(readCache.registerVolume(ltag,
                                         ClusterCacheMode::LocationBased));

    const OwnerTag ctag(2);
    const ClusterCacheHandle
        chandle(readCache.registerVolume(ctag,
                                         ClusterCacheMode::ContentBased));

    std::vector<std::unique_ptr<ClusterCacheKey>> keys;
    keys.resize(size);
    for(unsigned i = 0; i < size; ++i)
    {
        auto& handle = i % 2 ? lhandle : chandle;
        auto k(std::make_unique<ClusterCacheKey>(random_key(handle)));
        readCache.add(handle,
                      *k,
                      buf.data(),
                      buf.size());

        keys[i] = std::move(k);
    }

    for (unsigned i = 0; i < size; ++i)
    {
        ASSERT_TRUE(readCache.read(i % 2 ?
                                   lhandle :
                                   chandle,
                                   *keys[i],
                                   buf.data(),
                                   buf.size()));
    }

    for(unsigned i = 0; i < size; ++i)
    {
        const auto& handle = i % 2 ? lhandle : chandle;
        readCache.add(handle,
                      random_key(handle),
                      buf.data(),
                      buf.size());
    }

    for (unsigned i = 0; i < size; ++i)
    {
        ASSERT_FALSE(readCache.read(i % 2 ?
                                    lhandle :
                                    chandle,
                                    *keys[i],
                                    buf.data(),
                                    buf.size()));
    }
}

TEST_P(ClusterCacheSerializationTest, device_serialization_1_content_based)
{
    test_device_serialization_1(ClusterCacheMode::ContentBased);
}

TEST_P(ClusterCacheSerializationTest, device_serialization_1_location_based)
{
    test_device_serialization_1(ClusterCacheMode::LocationBased);
}

TEST_P(ClusterCacheSerializationTest, multiple_registrations_with_same_owner_tag)
{
    std::vector<uint8_t> buf(4096);
    yt::DimensionedValue v1("1GiB");
    ASSERT_TRUE(v1.getBytes() > (1 << 22));

    std::list<std::pair<std::string, uint64_t> > sizes;
    sizes.push_back(std::make_pair(UUID().str(), v1.getBytes()));

    bpt::ptree pt;

    fillConfigurationPropertyTree(pt,
                                  2,
                                  sizes);
    ClusterCacheType readCache(pt);

    const OwnerTag otag(1);
    const ClusterCacheHandle
        lhandle(readCache.registerVolume(otag,
                                         ClusterCacheMode::LocationBased));
    EXPECT_EQ(lhandle,
              readCache.registerVolume(otag,
                                       ClusterCacheMode::LocationBased));

    EXPECT_EQ(ClusterCacheHandle(0),
              readCache.registerVolume(otag,
                                       ClusterCacheMode::ContentBased));
}

TEST_P(ClusterCacheSerializationTest, multiple_deregistrations_with_same_owner_tag)
{
    std::vector<uint8_t> buf(4096);
    yt::DimensionedValue v1("1GiB");
    ASSERT_TRUE(v1.getBytes() > (1 << 22));

    std::list<std::pair<std::string, uint64_t> > sizes;
    sizes.push_back(std::make_pair(UUID().str(), v1.getBytes()));

    bpt::ptree pt;

    fillConfigurationPropertyTree(pt,
                                  2,
                                  sizes);
    ClusterCacheType readCache(pt);

    const OwnerTag otag(1);
    readCache.deregisterVolume(otag);

    readCache.registerVolume(otag,
                             ClusterCacheMode::LocationBased);

    readCache.deregisterVolume(otag);
    EXPECT_NO_THROW(readCache.deregisterVolume(otag));

    readCache.registerVolume(otag,
                             ClusterCacheMode::ContentBased);
    EXPECT_NO_THROW(readCache.deregisterVolume(otag));
    EXPECT_NO_THROW(readCache.deregisterVolume(otag));
}

TEST_P(ClusterCacheSerializationTest, make_ze_big_one_content_based)
{
    make_ze_big_one(ClusterCacheMode::ContentBased);
}

TEST_P(ClusterCacheSerializationTest, make_ze_big_one_location_based)
{
    make_ze_big_one(ClusterCacheMode::LocationBased);
}

TEST_P(ClusterCacheSerializationTest, make_ze_big_one_mixed)
{
    fs::path output = FileUtils::create_temp_file_in_temp_dir("KAKSerialize");
    ALWAYS_CLEANUP_FILE(output);

    std::vector<uint8_t> buf(4096);
    yt::DimensionedValue v1("5GiB");
    ASSERT_TRUE(v1.getBytes() > (1 << 22));

    const uint64_t read_cache_constant = v1.getBytes() >> 12;
    std::list<std::pair<std::string, uint64_t> > sizes;
    sizes.push_back(std::make_pair(UUID().str(), v1.getBytes()));

    bpt::ptree pt;

    fillConfigurationPropertyTree(pt,
                                  2,
                                  sizes,
                                  false);
    ClusterCacheType readCache(pt);


    const OwnerTag ltag(1);
    const ClusterCacheHandle
        lhandle(readCache.registerVolume(ltag,
                                         ClusterCacheMode::LocationBased));

    const OwnerTag ctag(2);
    const ClusterCacheHandle
        chandle(readCache.registerVolume(ctag,
                                         ClusterCacheMode::ContentBased));

    for(uint64_t i = 0; i < read_cache_constant; ++i)
    {
        auto& handle = i % 2 ? chandle : lhandle;
        readCache.add(handle,
                      random_key(handle),
                      buf.data(),
                      buf.size());
    }

    Serialization::serializeAndFlush<ClusterCacheType::oarchive_type>(output, readCache);

    {
        bpt::ptree pt;

        std::list<std::pair<std::string, uint64_t> > sizes;

        fillConfigurationPropertyTree(pt,
                                      2,
                                      sizes,
                                      false);
        ClusterCacheType readCache(pt);

        fs::ifstream ifs(output);
        ClusterCacheType::iarchive_type ia(ifs);
        ia >> readCache;
        ifs.close();
    }
}

TEST_P(ClusterCacheSerializationTest, ze_big_one_content_based)
{
    ze_big_one(ClusterCacheMode::ContentBased);
}

TEST_P(ClusterCacheSerializationTest, ze_big_one_location_based)
{
    ze_big_one(ClusterCacheMode::LocationBased);
}

TEST_P(ClusterCacheSerializationTest, ze_big_one_mixed)
{
    const int64_t max_test = 32;

    for(int f = 1; f <= max_test ; f *= 2)
    {
        std::cout << "<<<<<< test with " << f << " GiB >>>>>>>>" << std::endl;

        std::cout << sizeof(std::list<uint64_t>) << std::endl;

        fs::path output = FileUtils::create_temp_file_in_temp_dir("ClusterCacheSerializationTest");
        ALWAYS_CLEANUP_FILE(output);

        std::stringstream ss;
        // ss << r.IntegerC(10,50) << "GiB";
        ss << f << "GiB";

        yt::DimensionedValue v1(ss.str());
        ASSERT_TRUE(v1.getBytes() > (1 << 22));

        const uint64_t read_cache_constant = v1.getBytes() >> 12;
        const uint64_t vec_size = read_cache_constant >> 10;
        std::vector<std::unique_ptr<ClusterCacheKey>> cvec;
        std::vector<std::unique_ptr<ClusterCacheKey>> lvec;
        cvec.reserve(vec_size / 2);
        lvec.reserve(vec_size / 2);

        yt::cpu_timer ct;
        yt::wall_timer wt;

        std::vector<uint8_t> buf(4096);

        const OwnerTag ltag(1);
        const OwnerTag ctag(2);

        {
            std::list<std::pair<std::string, uint64_t> > sizes;
            sizes.push_back(std::make_pair(UUID().str(), v1.getBytes()));

            bpt::ptree pt;
            fillConfigurationPropertyTree(pt,
                                          2,
                                          sizes,
                                          false);
            ClusterCacheType readCache(pt);

            const ClusterCacheHandle
                lhandle(readCache.registerVolume(ltag,
                                                 ClusterCacheMode::LocationBased));

            const ClusterCacheHandle
                chandle(readCache.registerVolume(ctag,
                                                 ClusterCacheMode::ContentBased));

            std::cout << "Checking the keys" << std::endl;
            wt.restart();
            ct.restart();

            for(uint64_t i = 0; i < read_cache_constant; ++i)
            {
                auto& handle = i % 2 ? chandle : lhandle;

                auto k(std::make_unique<ClusterCacheKey>(random_key(handle)));
                readCache.add(handle,
                              *k,
                              buf.data(),
                              buf.size());

                auto& vec = i % 2 ? cvec : lvec;

                if(vec.size() < vec_size / 2 and
                   sou(1023) == 0)
                {
                    vec.push_back(std::move(k));
                }
            }

            std::cout << "created " << read_cache_constant << " keys in "
                      << ct.elapsed() << "seconds cpu time"
                      << wt.elapsed() << "seconds wall time" << std::endl;

            std::cout << "Checking " << lvec.size() << " location-based keys" << std::endl;

            for(uint64_t i = 0; i < lvec.size(); ++i)
            {
                ASSERT_TRUE(readCache.read(lhandle,
                                           *lvec[i],
                                           buf.data(),
                                           buf.size()));
            }

            std::cout << "Checking " << cvec.size() << " content-based keys" << std::endl;

            for(uint64_t i = 0; i < cvec.size(); ++i)
            {
                ASSERT_TRUE(readCache.read(chandle,
                                           *cvec[i],
                                           buf.data(),
                                           buf.size()));
            }
            wt.restart();
            ct.restart();

            std::cout << "Streaming out" << std::endl;

            Serialization::serializeAndFlush<ClusterCacheType::oarchive_type>(output,
                                                                              readCache);

            std::cout << "streamed out " << read_cache_constant << " keys in "
                      << ct.elapsed() << "seconds cpu time"
                      << wt.elapsed() << "seconds wall time" << std::endl;

        }
        // size of file 716800K
        {
            std::cout << "streaming in " << std::endl;
            bpt::ptree pt;
            std::list<std::pair<std::string, uint64_t> > sizes;

            fillConfigurationPropertyTree(pt,
                                          1,
                                          sizes,
                                          false);
            ClusterCacheType readCache(pt);

            fs::ifstream ifs(output);
            wt.restart();
            ct.restart();

            ClusterCacheType::iarchive_type ia(ifs);
            ia >> readCache;
            ifs.close();
            //   readCache.finishInitialization(2);
            std::cout << "streamed in " << read_cache_constant << " keys in "
                      << ct.elapsed() << "seconds cpu time"
                      << wt.elapsed() << "seconds wall time" << std::endl;

            const ClusterCacheHandle
                lhandle(readCache.registerVolume(ltag,
                                                 ClusterCacheMode::LocationBased));

            const ClusterCacheHandle
                chandle(readCache.registerVolume(ctag,
                                                 ClusterCacheMode::ContentBased));

            std::cout << "Checking keys again: " << std::endl;
            for(uint64_t i = 0; i < cvec.size(); ++i)
            {
                ASSERT_TRUE(readCache.read(chandle,
                                           *cvec[i],
                                           buf.data(),
                                           buf.size()));
            }
            for(uint64_t i = 0; i < lvec.size(); ++i)
            {
                ASSERT_TRUE(readCache.read(lhandle,
                                           *lvec[i],
                                           buf.data(),
                                           buf.size()));
            }
        }
    }
}

TEST_P(ClusterCacheSerializationTest, limited_entries_serialization)
{
    const size_t capacity = 8;
    std::unique_ptr<ClusterCacheType> cache(make_cache(capacity));

    const OwnerTag ctag(1);
    const ClusterCacheHandle
        chandle(cache->registerVolume(ctag,
                                      ClusterCacheMode::ContentBased));

    const size_t csize = 3;
    cache->set_max_entries(chandle,
                           csize);
    std::vector<ClusterCacheKey> ckeys(csize,
                                       ClusterCacheKey(yt::Weed::null()));

    const OwnerTag ltag(2);

    ASSERT_NE(ltag,
              ctag);

    const ClusterCacheHandle
        lhandle(cache->registerVolume(ltag,
                                      ClusterCacheMode::LocationBased));
    const size_t lsize = 4;
    cache->set_max_entries(lhandle,
                           lsize);


    auto check_nspaces([&]
                       {
                           const std::vector<ClusterCacheHandle>
                               nspaces(cache->list_namespaces());
                           EXPECT_EQ(2U,
                                     nspaces.size());
                           for (const auto& h : nspaces)
                           {
                               EXPECT_TRUE(h == chandle or
                                           h == lhandle);
                           }
                       });

    check_nspaces();

    std::vector<ClusterCacheKey> lkeys(lsize,
                                       ClusterCacheKey(yt::Weed::null()));

    std::vector<uint8_t> buf(4096);

    for (size_t i = 0; i < capacity; ++i)
    {
        {
            ClusterCacheKey ckey(random_key(chandle));
            cache->add(chandle,
                       ckey,
                       buf.data(),
                       buf.size());

            ckeys[i % csize] = ckey;
        }

        {
            ClusterCacheKey lkey(random_key(lhandle));
            cache->add(lhandle,
                       lkey,
                       buf.data(),
                       buf.size());

            lkeys[i % lsize] = lkey;
        }
    }

    auto check_limits([&]
                      {
                          const ClusterCacheType::NamespaceInfo
                              cinfo(cache->namespace_info(chandle));
                          EXPECT_EQ(csize,
                                    *cinfo.max_entries);
                          ASSERT_TRUE(cinfo.max_entries != boost::none);
                          EXPECT_EQ(csize,
                                    cinfo.entries);

                          const ClusterCacheType::NamespaceInfo
                              linfo(cache->namespace_info(lhandle));
                          EXPECT_EQ(lsize,
                                    *linfo.max_entries);
                          ASSERT_TRUE(linfo.max_entries != boost::none);
                          EXPECT_EQ(lsize,
                                    linfo.entries);
                      });

    check_limits();

    bpt::ptree pt;
    cache->persist(pt,
                   ReportDefault::T);

    cache.reset();

    cache = std::make_unique<ClusterCacheType>(pt);

    check_nspaces();
    check_limits();

    EXPECT_EQ(chandle,
              cache->registerVolume(ctag,
                                    ClusterCacheMode::ContentBased));
    EXPECT_EQ(lhandle,
              cache->registerVolume(ltag,
                                    ClusterCacheMode::LocationBased));

    check_nspaces();
    check_limits();

    for (const auto& c : ckeys)
    {
        EXPECT_TRUE(cache->read(chandle,
                                c,
                                buf.data(),
                                buf.size()));
    }

    for (const auto& l : lkeys)
    {
        EXPECT_TRUE(cache->read(lhandle,
                                l,
                                buf.data(),
                                buf.size()));
    }
}

INSTANTIATE_TEST(ClusterCacheSerializationTest);

}

BOOST_CLASS_VERSION(volumedrivertest::ClusterCacheFakeStore, 1);
BOOST_CLASS_VERSION(volumedrivertest::FakeDevice, 0);
BOOST_CLASS_VERSION(volumedriver::ClusterCacheT<volumedrivertest::FakeDevice>, 2);
BOOST_CLASS_VERSION(volumedriver::ClusterCacheDeviceManagerT<volumedrivertest::FakeDevice>, 2);

// Local Variables: **
// mode: c++ **
// End: **
