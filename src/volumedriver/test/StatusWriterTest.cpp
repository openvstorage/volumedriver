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

#include "VolManagerTestSetup.h"
#include "../StatusWriter.h"
#include "../ScopedVolume.h"

namespace volumedrivertest
{
using namespace volumedriver;

class StatusWriterTest
    : public VolManagerTestSetup
{
public:
    StatusWriterTest()
        : VolManagerTestSetup("StatusWriterTest")
    {}


};

TEST_P(StatusWriterTest, write_a_bunch_of_crap)
{
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns = ns1_ptr->ns();

    // Namespace ns;

    ScopedWriteOnlyVolumeWithLocalDeletion v(newWriteOnlyVolume("vol_name",
                                                                ns,
                                                                VolumeConfig::WanBackupVolumeRole::WanBackupBase));


    volumedriver_backup::StatusWriter t(boost::posix_time::seconds(1),
                                        ns);

    const unsigned test_size = 128;
    const unsigned cluster_size = v->getClusterSize();

    t.start(v.get(),
            test_size * cluster_size);

    for(unsigned i = 0; i < test_size; ++i)
    {
        t.add_seen(cluster_size);
        t.add_kept(cluster_size);

        writeToVolume(v.get(),
                      0,
                      cluster_size,
                      "immanuel");


        usleep(50000);
    }

    t.finish();

}

INSTANTIATE_TEST(StatusWriterTest);
}

// Local Variables: **
// mode: c++ **
// End: **
