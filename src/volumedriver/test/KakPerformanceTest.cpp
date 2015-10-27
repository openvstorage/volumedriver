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

#if IMMANUEL
#include "ExGTest.h"
//#include <timer.h>
#include <fstream>
#include <boost/ptr_container/ptr_vector.hpp>
#include <RandomLib/Random.hpp>
#include <boost/filesystem/fstream.hpp>
#include "ClusterCacheMap.h"
#include "ClusterCache.h"

namespace volumedrivertest
{
using namespace volumedriver;
using namespace RandomLib;




class KaKPerformanceTest : public ExGTest
{
public:
    KaKPerformanceTest()
        : io (0)
    {}


    template<typename T>
    T*
    randomEntry()
    {
        T* e = new T();
        uint64_t* zint = reinterpret_cast<uint64_t*>(e->key.weed_);
        for(unsigned i = 0; i < Weed::weed_size / sizeof(uint64_t); ++i)
        {
            *(zint+i) = r.Integer<uint64_t>();
        }
        return e;
    }

    void
    make_the_big_kak(uint64_t /*size2*/,
                     const fs::path& filename)
    {
        fs::ofstream ofs(filename);
        // KontentAddressedKache::oarchive_type oa(ofs);

        // for(unsigned i = 0; i < size2; ++i)
        // {
        //     KontentAddressedKacheEntry* e = randomEntry<KontentAddressedKacheEntry>();
        //     oa & *e;
        //     delete e;
        // }
    }


    void
    SetUp()
    {
        r.Reseed();
    }

    void
    TearDown()
    {
    }

    void
    getNumberOfWeedsPerSecond()
    {
        if(number_of_weeds_per_second == 0)
        {
            cpu_timer wt;
            for(int i = 0; i < 1024 * 1024; ++i)
            {
                // KontentAddressedKacheEntry e;
                //                randomize(e);

            }
            number_of_weeds_per_second = 1024.0 * 1024.0 / wt.elapsed();
        }
    }

    double
    getTimeForWeeds(const uint64_t weeds)
    {
        std::vector<KontentAddressedKacheEntry* > vec(1024*weeds);
        cpu_timer wt;
        for(unsigned i = 0; i < 1024 * weeds; ++i)
        {
            KontentAddressedKacheEntry* e = randomEntry<KontentAddressedKacheEntry>();
            delete(e);

            //            vec[i] = e;
        }
        double ret = wt.elapsed() / 1024.0;
        // BOOST_FOREACH(KontentAddressedKacheEntry* e, vec)
        // {
        //     delete e;
        // }
        return ret;
    }



    void
    print_plot(const std::string& fname,
               std::vector<double>& vals)
    {
        FileUtils::removeFileNoThrow(fname);
        std::ofstream os(fname);
        os << "\\begin{code}" << std::endl;

        os << "module Main where" << std::endl
           << "import qualified Graphics.Gnuplot.Plot.TwoDimensional as Plot2D" << std::endl
           << "import qualified Graphics.Gnuplot.Graph.TwoDimensional as Graph2D" << std::endl
           << "import qualified Graphics.Gnuplot.Terminal.X11 as X11" << std::endl
           << "import qualified Graphics.Gnuplot.Terminal.PNG as PNG" << std::endl
           << "import qualified Graphics.Gnuplot.Advanced as Plot" << std::endl
           << "list2d :: Plot2D.T Int Double" << std::endl
           << "list2d = Plot2D.list Graph2D.listPoints [" ;

        for(unsigned i = 0; i < vals.size() - 1 ; ++i)
        {
            os << vals[i] << ", " ;
        }
        if(vals.size() > 0)
        {
            os << vals.back();
        }
        os << "]" << std:: endl;

        os << "view = do" << std::endl
           << " Plot.plot X11.cons list2d" << std::endl;
        os << "print = do" << std::endl
           << "Plot.plot (PNG.cons \"" << fname << ".png\")" << " list2d" << std::endl;

        os << "\\end{code}" << std::endl;

    }

private:
    FILE* io;
    const static uint64_t four_k = 4096;
    RandomLib::Random r;
protected:
    //    KontentAddressedKache cache;
    static uint64_t number_of_weeds_per_second;
    DECLARE_LOGGER("KaKPerformanceTest");


    // typedef bi::list<KontentAddressedKacheEntry,
    //              bi::constant_time_size<false> > lrulist_t;

    // typedef bi::set<KontentAddressedKacheEntry,
    //             bi::compare<std::greater<KontentAddressedKacheEntry> >,
    //             bi::constant_time_size<false> > cachemap_t;

};

uint64_t
KaKPerformanceTest::number_of_weeds_per_second = 0;

TEST_F( KaKPerformanceTest, DISABLED_test0)
{
    std::vector<double> results;
    const uint64_t size2 = 4096;
    std::vector<KontentAddressedKacheEntry*> vec(size2 * 1024);

    ASSERT_TRUE(vec.size() == 1024*size2);
    cpu_timer wt;
    for(uint64_t i = 0; i < 1024*size2; ++i)
    {
        KontentAddressedKacheEntry* e = randomEntry<KontentAddressedKacheEntry>();
        vec[i] = e;

        if(i % size2 == (size2-1))
        {
            results.push_back(wt.elapsed());
            wt.restart();
        }
    }

    ASSERT_TRUE(vec.size() == 1024*size2);

    print_plot("vec_size.lhs", results);
}

TEST_F( KaKPerformanceTest, DISABLED_test1)
{
    std::vector<double> results;

    //    LOG_FATAL("Number of weeds per sec: " << number_of_weeds_per_second);
    const uint64_t size2 = 4096;
    double fixed_time = getTimeForWeeds(size2);
    std::cout << "time to allocate " << size2 << " entries " << fixed_time << std::endl;
    lrulist_t lru_;

    cpu_timer wt;
    for(uint64_t i = 0; i < 1024*size2; ++i)
    {
        KontentAddressedKacheEntry* entry = randomEntry<KontentAddressedKacheEntry>();
        lru_.push_back(*entry);


        if(i % size2 == (size2-1))
        {
            results.push_back(wt.elapsed());
            wt.restart();
        }
    }
    ASSERT_TRUE(lru_.size() == 1024*size2);

    print_plot("lru_size.lhs", results);
}

TEST_F( KaKPerformanceTest, test2)
{
    std::vector<double> results;

    const uint64_t size2 = 4096;

    cachemap_t cache_map;

    cpu_timer wt;
    for(uint64_t i = 0; i < 1024*size2; ++i)
    {
        KontentAddressedKacheEntry* entry = randomEntry<KontentAddressedKacheEntry>();
        cache_map.insert(*entry);
        delete entry;

        if(i % size2 == (size2 - 1))
        {
            results.push_back(wt.elapsed());
            wt.restart();
        }
    }
    ASSERT_TRUE(cache_map.size() == 1024*size2);
    print_plot("map_times.lhs", results);
}

TEST_F(KaKPerformanceTest, DISABLED_test3)
{

    std::vector<double> serialize;
    std::vector<double> deserialize;
    std::vector<double> filesize;

    for(unsigned x = 0;  x < 8; ++x)
    {
        const uint64_t size2 = 1024 << x;


        fs::path kakpath = FileUtils::create_temp_file_in_temp_dir("kakserialization");
        ALWAYS_CLEANUP_FILE(kakpath);
        cpu_timer wt;
        make_the_big_kak(size2,
                         kakpath);

        //        std::cout << "time to serialize " << size2 << " entries: " << wt.elapsed() << std::endl;
        serialize.push_back(wt.elapsed()/size2);
        sync();

        fs::ifstream ifs(kakpath);
        KontentAddressedKache::iarchive_type ia(ifs);
        filesize.push_back(fs::file_size(kakpath) / size2);

        wt.restart();

        for(unsigned i = 0 ; i < size2; ++i)
        {
            std::unique_ptr<KontentAddressedKacheEntry> entry(new KontentAddressedKacheEntry());
            ia & *entry;
        }
        deserialize.push_back(wt.elapsed() / size2);
        //        std::cout << "time to deserialize " << size2 << " entries: " << wt.elapsed() << std::endl;
    }
    print_plot("serialize.lhs", serialize);
    print_plot("deserialize.lhs", deserialize);
    print_plot("filesize.lhs", filesize);

}

TEST_F(KaKPerformanceTest, DISABLED_test4)
{
    std::vector<double> res;
    const uint64_t size2 = 4096;
    ClusterCacheMap<ClusterCache::ClusterCacheEntry,
                    SListClusterCacheEntryValueTraitsOption> map;
    cpu_timer wt;
    std::cout << "cpu timer resolution: " << wt.resolution() << std::endl;

    map.resize(22);
    std::cout << "Time to allocate map of " << (1 << 22) << "entries: " << wt.elapsed() << std::endl;
    wt.restart();

    for(uint64_t i = 0; i < 1024* size2; ++i)
    {
        ClusterCache::ClusterCacheEntry* e = randomEntry<ClusterCache::ClusterCacheEntry>();
        map.insert(e);
        delete e;

        if(i % size2 == (size2 - 1))
        {
            res.push_back(wt.elapsed());
            wt.restart();
        }
    }
    ASSERT_TRUE(map.entries() == 1024*size2);
    print_plot("readcachemap_times.lhs", res);
}

TEST_F(KaKPerformanceTest, DISABLED_test5)
{
    std::vector<double> res;
    const uint64_t size2 = 4096;
    ClusterCacheMap<ClusterCache::ClusterCacheEntry,
                    SListClusterCacheEntryValueTraitsOption> map;
    cpu_timer wt;
    std::cout << "cpu timer resolution: " << wt.resolution() << std::endl;

    map.resize(22);
    std::cout << "Time to allocate map of " << (1 << 22) << "entries: " << wt.elapsed() << std::endl;
    wt.restart();

    for(uint64_t i = 0; i < 1024* size2; ++i)
    {
        ClusterCache::ClusterCacheEntry* e = randomEntry<ClusterCache::ClusterCacheEntry>();
        if(!map.find(e->key))
        {
            map.insert(e);
        }
        delete e;

        if(i % size2 == (size2 - 1))
        {
            res.push_back(wt.elapsed());
            wt.restart();
        }
    }
    ASSERT_TRUE(map.entries() == 1024*size2);
    print_plot("readcachemap_find_insert.lhs", res);

}
}
#endif

// im Variables: **
// mode: c++ **
// End: **
