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

#include "VolumeDriverTestConfig.h"
#include <tcutil.h>
#include <tcbdb.h>
#include <tcfdb.h>


#include "youtils/FileUtils.h"

namespace volumedrivertest
{
using namespace volumedriver;
namespace fs = boost::filesystem;

class GeneralTest : public testing::TestWithParam<VolumeDriverTestConfig>
{};

#define HANDLE(x) if(not x)                                             \
    {                                                                   \
        std::string msg("problem with tc: ");                           \
        if(database_)                                                   \
        {                                                               \
            int  ecode = tcbdbecode(database_);                         \
            msg += tcbdberrmsg(ecode);                                  \
            std::cerr << msg << std::endl;                              \
            ASSERT(0==1);                                               \
        }                                                               \
    }

TEST_F(GeneralTest, DISABLED_memcpy_test)
{
    const fs::path filename("/tmp/metadatstoretest");

    TCBDB* database_(tcbdbnew());
    HANDLE(tcbdbsetcache(database_,
                  0,
                         1024));

    HANDLE(tcbdbsetcmpfunc(database_,tccmpint64,NULL));
    HANDLE(tcbdbopen(database_, filename.file_string().c_str(), BDBOWRITER));

    for(int page_size = 10; page_size < 17; ++page_size)
    {
        const uint64_t size = 1 << page_size;
        const uint64_t times = 1 << 10;
        std::vector<void*> bufs1;
        const uint64_t num_bufs = 1 << 10;

        void * b1 = valloc(size);
        for(uint64_t i = 0; i < num_bufs; ++i)
        {

            HANDLE(tcbdbput(database_,&i, sizeof(i),b1, size));


        }

        youtils::wall_timer wt;

        for(uint64_t i = 0; i < times; ++i)
        {
            for(uint64_t j = 0; j < num_bufs; j++)
            {
                int _sz = 0;
                const void* buf = tcbdbget3(database_, &j, sizeof(j), &_sz);
                ASSERT((unsigned)_sz == size);
                memcpy(b1, buf, size);
                ((uint64_t*)b1)[3] = 15;
                tcbdbput(database_,&j, sizeof(j), buf, size);
            }
        }
        double time = wt.elapsed();
        std::cout << "page_size is " << size << std::endl;

        std::cout << "To copy " << size
                  << " bytes " << times * num_bufs
                  << " times  takes "
                  << time << " seconds" << std::endl;

        std::cout << "this limit voldrv performance to " << (1 << 12) * (times*num_bufs) / ((1 << 21) * time) << "Meg per second" << std::endl ;

    // for(uint64_t i = 0; i < num_bufs; ++i)
    // {
    //     free(buf1);
    //     free(buf2);
    // }
    }

}
//INSTANTIATE_TEST(GeneralTest);

}


// Local Variables: **
// mode: c++ **
// End: **
