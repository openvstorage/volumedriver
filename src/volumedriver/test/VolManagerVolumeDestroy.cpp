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
#include "../VolManager.h"
#include "../TLogReader.h"
#include "../failovercache/fungilib/File.h"

namespace volumedriver
{

class VolManagerVolumeDestroy
    : public VolManagerTestSetup
{
public:
    VolManagerVolumeDestroy()
        : VolManagerTestSetup("VolManagerVolumeDestroy")
    {
        // dontCatch(); uncomment if you want an unhandled exception to cause a crash, e.g. to get a stacktrace
    }
};

TEST_P(VolManagerVolumeDestroy, one)
{
    const std::string volume = "volume";
    //    const backend::Namespace namespace1;
    auto ns1_ptr = make_random_namespace();
    const Namespace& ns1 = ns1_ptr->ns();


    SharedVolumePtr v = newVolume(volume,
			  ns1);
    //setTLogMaxEntries(v, 3);

    writeToVolume(*v, 0, 4096, "g");
    writeToVolume(*v, 0, 4096, "g");
    writeToVolume(*v, 0, 4096, "g");

    v->sync();
    ::sync();

    const std::string ssx = snapshotFilename();;
    const OrderedTLogIds tlog_ids(v->getSnapshotManagement().getCurrentTLogs());
    waitForThisBackendWrite(*v);
    // /tmp/VolManagerVolumeDestroy/localbackend/namespace1/tlog_00_0000000000000001
    // /tmp/VolManagerVolumeDestroy/localbackend/namespace1/snapshots.xml_00
    const std::string localBackendDir = "/tmp/VolManagerVolumeDestroy/localbackend/";
    const std::string namespaceDir = localBackendDir + ns1.str() + "/";


    //    const std::string tlog1 = namespaceDir + "tlog_00_0000000000000001";
    //    const std::string tlog2 = namespaceDir + "tlog_00_0000000000000002";
    const std::string assx = namespaceDir + ssx;
    for(unsigned i = 0; i < tlog_ids.size() - 1; i++)
    {
        EXPECT_TRUE(fungi::File::exists(namespaceDir + boost::lexical_cast<std::string>(tlog_ids[i])));
    }

    EXPECT_FALSE(fungi::File::exists(namespaceDir + boost::lexical_cast<std::string>(tlog_ids[tlog_ids.size() - 1])));
    // Y42 there will be discussion about this.
    //    EXPECT_TRUE(fungi::File::exists(assx));

    destroyVolume(v,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::T);

//     EXPECT_FALSE(fungi::File::exists(tlog1));
//     EXPECT_FALSE(fungi::File::exists(tlog2));
//     EXPECT_FALSE(fungi::File::exists(assx));
}

INSTANTIATE_TEST(VolManagerVolumeDestroy);

}

// Local Variables: **
// mode: c++ **
// End: **
