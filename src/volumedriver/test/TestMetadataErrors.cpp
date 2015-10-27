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

namespace volumedrivertest
{
using namespace volumedriver;

class TestMetadataErrors
    : public VolManagerTestSetup
{
public:
    TestMetadataErrors()
        : VolManagerTestSetup("TestMetadataErrors",
                              UseFawltyMDStores::T,
                              UseFawltyTLogStores::T)
    {}
};

TEST_P(TestMetadataErrors, testHaltingVolumeWhenWritingToSnapshotsFailsInMgmtPath)
{
    const std::string id1("id1");
    auto ns_ptr = make_random_namespace();
    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v1 = VolManagerTestSetup::newVolume(id1,
                                                ns);

    writeToVolume(v1,
                  0,
                  4096,
                  "kutmetperen");

    ASSERT_NO_THROW(v1->createSnapshot("snap1"));
    waitForThisBackendWrite(v1);
    writeToVolume(v1,
                  0,
                  4096,
                  "kutmetperen");

    const std::string path_regex = "/" + ns.str() + "/snapshot.*";
    add_failure_rule(mdstores_,
                     1,
                     path_regex,
                     {fawltyfs::FileSystemCall::Write},
                     0,
                     10,
                     5);
    ASSERT_THROW(v1->createSnapshot("snap2"),
                 youtils::SerializationFlushException);
    ASSERT_TRUE(v1->is_halted());

}

TEST_P(TestMetadataErrors, testHaltingVolumeWhenWritingToSnapshotsFailsInIOPath)
{
    const std::string id1("id1");
    auto ns_ptr = make_random_namespace();
    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v1 = VolManagerTestSetup::newVolume(id1,
                                                ns);

    writeToVolume(v1,
                  0,
                  4096,
                  "kutmetperen");

    ASSERT_NO_THROW(v1->createSnapshot("snap1"));
    waitForThisBackendWrite(v1);

    const std::string path_regex = "/" + ns.str() + "/snapshot.*";
    add_failure_rule(mdstores_,
                     1,
                     path_regex,
                     {fawltyfs::FileSystemCall::Write},
                     0,
                     10,
                     5);

    // 20 scos of 16Meg each 4K*4*1024*20
    unsigned tlog_rollover_size =  VolManager::get()->number_of_scos_in_tlog.value() * v1->getSCOMultiplier();

    for(unsigned i = 0; i < (tlog_rollover_size - 1); ++i)
    {
        writeToVolume(v1, 0, 4096, "kutmetperen");
    }

    ASSERT_THROW(writeToVolume(v1,0, 4096, "kutmetperen"),
                 youtils::SerializationFlushException);
    ASSERT_TRUE(v1->is_halted());
}

TEST_P(TestMetadataErrors, testHaltingVolumeWhenWritingToTlogFailsInMgmtPath)
{
    const std::string id1("id1");
    auto ns_ptr = make_random_namespace();
    const backend::Namespace& ns = ns_ptr->ns();

    Volume* v1 = VolManagerTestSetup::newVolume(id1,
                                                ns);

    writeToVolume(v1,
                  0,
                  4096,
                  "kutmetperen");

    ASSERT_NO_THROW(v1->createSnapshot("snap1"));

    writeToVolume(v1,
                  0,
                  4096,
                  "kutmetperen");
    waitForThisBackendWrite(v1);

    const std::string path_regex = "/" + ns.str() + "/tlog_.*";

    add_failure_rule(tlogs_,
                     1,
                     path_regex,
                     {fawltyfs::FileSystemCall::Write},
                     0,
                     10,
                     5);

    EXPECT_THROW(v1->createSnapshot("snap2"),
                 youtils::FileDescriptorException);
    EXPECT_TRUE(v1->is_halted());
    // here because otherwise the destroy fails.
    remove_failure_rule(tlogs_,
                        1);


}

TEST_P(TestMetadataErrors, testHaltingVolumeWhenWritingToTlogFailsInIOPath)
{
    const std::string id1("id1");
    auto ns_ptr = make_random_namespace();
    const backend::Namespace& ns = ns_ptr->ns();


    Volume* v1 = VolManagerTestSetup::newVolume(id1,
                                                ns);

    writeToVolume(v1,
                  0,
                  4096,
                  "kutmetperen");

    ASSERT_NO_THROW(v1->createSnapshot("snap1"));

    writeToVolume(v1,
                  0,
                  4096,
                  "kutmetperen");
    waitForThisBackendWrite(v1);

    const std::string path_regex = "/" + ns.str() + "/tlog_.*";

    add_failure_rule(tlogs_,
                     1,
                     path_regex,
                     {fawltyfs::FileSystemCall::Write},
                     0,
                     10,
                     5);

    unsigned tlog_rollover_size =  VolManager::get()->number_of_scos_in_tlog.value() * v1->getSCOMultiplier();
    bool caught_low_level_file_exception = false;

    try
    {

       for(unsigned i = 0; i < tlog_rollover_size; ++i)
       {
            writeToVolume(v1, 0, 4096, "kutmetperen");
        }
    }
    catch(youtils::FileDescriptorException& e)
    {
        caught_low_level_file_exception = true;
    }
    catch(...)
    {
    }


    EXPECT_TRUE(caught_low_level_file_exception);
    EXPECT_TRUE(v1->is_halted());
    remove_failure_rule(tlogs_,
                        1);
}

INSTANTIATE_TEST(TestMetadataErrors);
}
