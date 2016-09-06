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

#include "../FileUtils.h"
#include "../System.h"
#include <../wall_timer.h>
#include "../Weed.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <boost/filesystem/fstream.hpp>
#include <boost/foreach.hpp>
#include <boost/timer.hpp>

namespace youtilstest
{

using namespace youtils;

class WeedTest
    : public testing::Test
{
protected:
    size_t
    stream_chunk_size()
    {
        return Weed::stream_chunk_size_;
    }
};

// class FingerPrintTest : public testing::Test
// {};


TEST_F(WeedTest, test1)
{
    std::vector<uint8_t> v1(4096);

    Weed h1(&v1[0], v1.size());
    Weed h2(&v1[0], v1.size());
    ASSERT_TRUE(h1 == h2);
    Weed h3(h2);
    ASSERT_TRUE(h1 == h3);
    h2 = h3;
    ASSERT_TRUE(h1 == h2);
}

TEST_F(WeedTest, greaterthan)
{
    std::vector<uint8_t> v1(4096,0);
    std::vector<uint8_t> v2(4096,1);
    Weed w1(&v1[0], v1.size());
    Weed w2(&v2[0], v2.size());
    ASSERT_TRUE(v1 < v2);
}

// static void
// getMD5FromSystem(const byte* buf,
//                  std::string& weed)
// {
//     fs::path p = FileUtils::create_temp_file_in_temp_dir("md5temp");
//     ALWAYS_CLEANUP_FILE(p);
//     fs::ofstream ofs(p);
//     ofs.write((char*)buf,4096);
//     ofs.close();
//     std::string command("md5sum ");
//     command += p.file_string();

//     FILE* output = popen(command.c_str(), "r");
//     if(!output)
//     {
//         throw fungi::IOException("could not popen md5sum");
//     }
//     char buf2[2*Weed::weed_size];
//     if(fread(buf2,1,2*Weed::weed_size,output) != 2*Weed::weed_size)
//     {
//         throw fungi::IOException("could not read from md5sum");
//     }
//     pclose(output);

//     weed = std::string(buf2, 2*Weed::weed_size);
// }

// TEST_F(FingerPrintTest, DISABLED_performance_test)
// {
//     const uint64_t num_bufs = 512*1024;

//     std::vector<byte*> randoms;
//     std::ifstream rand("/dev/urandom");
//     std::vector<Weed> weeds;
//     for(uint64_t i = 0; i < num_bufs ; ++i)
//     {
//         uint8_t* buf = (uint8_t*)malloc(4096);
//         rand.read((char*)buf, 4096);
//         ASSERT_TRUE(rand);
//         std::string weed;
//         getMD5FromSystem(buf,
//                          weed);

//         randoms.push_back(buf);
//         weeds.push_back(Weed(weed));
//     }




//     boost::timer t;
//     youtils::wall_timer t2;

//     for(uint64_t i = 0; i < 1024*1024; ++ i)
//     {
//         Weed w(randoms[i%num_bufs]);
//         if(i < num_bufs)
//         {
//             weeds.push_back(w);
//         }
//         else
//         {
//             ASSERT_TRUE(w == weeds[i%num_bufs]);
//         }
//     }

//     std::cout << "Time to calculate 1MiB hashes " << t.elapsed() << std::endl;
//     std::cout << "this is " << (4096 / t.elapsed()) << " MiB/sec" << std::endl;
//     std::cout << "this is " << (4096 / t2.elapsed()) << " MiB/sec in wall time" << std::endl;
//     BOOST_FOREACH(auto rand, randoms)
//     {
//         free(rand);
//     }
// }


TEST_F(WeedTest, correctness)
{
    std::vector<uint8_t> v1(4096,0);
    Weed h1(&v1[0], v1.size());
    //    std::cout << h1 << std::endl;

    Weed h2("620f0b67a91f7f74151bc5be745b7110");
    ASSERT_TRUE(h1 == h2);
}

TEST_F(WeedTest, timing)
{
    const size_t size = 10000;

    std::vector<Weed> weedes;
    weedes.reserve(size);
    std::vector<uint8_t> v1(4096);
    for(size_t i = 0; i < size; ++i)
    {
        Weed h(&v1[0], v1.size());
        ++v1[i%4096];
        weedes.push_back(h);
    }
    ASSERT_TRUE(weedes.size() == size);

    for(auto i = weedes.begin(); i != weedes.end(); ++i)
    {
        for(auto j = i+1; j != weedes.end(); j++)
        {
            ASSERT_FALSE(*i == *j);
        }
    }
}

TEST_F(WeedTest, stringification)
{
    std::vector<byte> v(4096, 7);
    const Weed w1(&v[0], v.size());
    std::stringstream ss;
    ss << w1;

    const Weed w2(ss.str());
    EXPECT_TRUE(w1 == w2);
}

TEST_F(WeedTest, serialization)
{
    std::vector<byte> v(4096, 7);

    for(int i = 0; i < 1024; i++)
    {
        const Weed w1(&v[0], v.size());
        std::stringstream oss;
        youtils::Serialization::serializeAndFlush<Weed::oarchive_type>(oss, w1);

        std::istringstream iss(oss.str());
        Weed::iarchive_type ia(iss);
        Weed w2;
        ia & w2;
        ASSERT_TRUE(w1 == w2);
        v[i]++;
    }
}

TEST_F(WeedTest, performance)
{
    uint64_t count = System::get_env_with_default("NUM_WEEDS",
                                                  1ULL << 10);

    std::vector<uint8_t> buf(4096);

    wall_timer wt;
    for (uint64_t i = 0; i < count; ++i)
    {
        *reinterpret_cast<uint64_t*>(buf.data()) = count;
        Weed w(buf);
    }

    double t = wt.elapsed();

    std::cout << "calculating " << count << " hashes for 4k buffers took " << t <<
        " seconds => " << (count * buf.size() / (t * 1024 * 1024)) << " MiB/s" << std::endl;
}

TEST_F(WeedTest, from_stream)
{
    const std::string s("MdFive");

    std::stringstream ss;
    ss << s;

    EXPECT_EQ(Weed(reinterpret_cast<const uint8_t*>(s.data()),
                       s.size()),
              Weed(ss));
}

TEST_F(WeedTest, from_stream_bigger)
{
    const std::vector<char> v(stream_chunk_size() + 1,
                              'Q');
    std::stringstream ss;

    ss.write(v.data(),
             v.size());

    EXPECT_EQ(Weed(reinterpret_cast<const uint8_t*>(v.data()),
                   v.size()),
              Weed(ss));
}

}
// Local Variables: **
// End: **
