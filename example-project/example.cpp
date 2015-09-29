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

#include "volumedriver/Api.h"

using namespace volumedriver;


#define MANAGEMENT_LOCK fungi::ScopedLock __l(api::getManagementMutex())
int
main(int,
     char**)
{

    api::LogInit();

    const boost::filesystem::path configuration_file("./volumedriver.json");

    api::Init(configuration_file,
         0);

    DimensionedValue twelve_gigs("12GiB");

    VolumeId id("example-volume");
    Namespace ns("example-namespace");
    Volume* vol = 0;

    {

        MANAGEMENT_LOCK;

        api::createNewVolume(id,
                             ns,
                             VolumeSize(twelve_gigs.getBytes()),
                             SCOMultiplier(4096),
                             volumedriver::VolumeConfig::default_lba_size_(),
                             volumedriver::VolumeConfig::default_cluster_mult_(),
                             0,
                             std::numeric_limits<uint64_t>::max(),
                             std::unique_ptr<const backend::Guid>(new backend::Guid()));
        vol = api::getVolumePointer(id);
    }

    assert(vol);


    const size_t buf_size(4096);

    std::vector<uint8_t> buf(buf_size, 'a');

    for(uint64_t i = 0; i < 512; ++i)
    {

        api::Write(vol,
                   0,
                   &buf[0],
                   buf_size);
    }


    api::Exit();

}

// Local Variables: **
// compile-command: "make" **
// End: **
