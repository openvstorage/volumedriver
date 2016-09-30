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

#include <sstream>

#include <boost/filesystem/fstream.hpp>
#include <boost/foreach.hpp>
#include <boost/scope_exit.hpp>

#include "backend/BackendException.h"
#include "VolManagerTestSetup.h"
#include "../SCOAccessData.h"
#include "../VolManager.h"

namespace volumedriver
{
using namespace backend;

class SCOAccessDataTest
    : public VolManagerTestSetup
{
public:
    SCOAccessDataTest()
        : VolManagerTestSetup("SCOAccessDataTest")
    {}

    std::unique_ptr<SCOAccessData>
    makeSCOAccessData(const Volume& v)
    {
        return std::unique_ptr<SCOAccessData>(new SCOAccessData(v.getNamespace(),
                                                                v.readActivity()));
    }

    void
    createAndPutSCOAccessData(size_t size)
    {
        const std::string volname("volume1");
        auto ns_ptr = make_random_namespace();

        const backend::Namespace& nspace = ns_ptr->ns();

        // const backend::Namespace nspace;

        SharedVolumePtr v = newVolume(volname,
                                      nspace);

        std::unique_ptr<SCOAccessData> sad(makeSCOAccessData(*v));
        sad->reserve(size);

        for(size_t i = 0; i < size; ++i)
        {
            sad->addData(SCO(i), 0.0);
        }

        SCOAccessDataPersistor sadp(v->getBackendInterface()->cloneWithNewNamespace(nspace));
        sadp.push(*sad,
                  v->backend_write_condition());

        destroyVolume(v,
                      DeleteLocalData::T,
                      RemoveVolumeCompletely::F);
    }

    void
    pullInexistentPtr(bool must_exist)
    {
        const std::string volname("volume");
        // const backend::Namespace nspace;
        auto ns_ptr = make_random_namespace();

        const backend::Namespace& nspace = ns_ptr->ns();

        SharedVolumePtr v = newVolume(volname,
                              nspace);

        BackendInterfacePtr bi(v->getBackendInterface()->cloneWithNewNamespace(nspace));
        SCOAccessDataPersistor sadp(std::move(bi));
        SCOAccessDataPtr sad;

        if (must_exist)
        {
            EXPECT_THROW(sad = sadp.pull(must_exist),
                         BackendObjectDoesNotExistException);
        }
        else
        {
            EXPECT_NO_THROW(sad = sadp.pull());
            EXPECT_EQ(nspace, sad->getNamespace());
            EXPECT_EQ(0U, sad->getVector().size());
        }
    }

    void
    pullInexistentRef(bool must_exist)
    {
        const std::string volname("volume");
        auto ns_ptr = make_random_namespace();

        const backend::Namespace& nspace = ns_ptr->ns();

        // const backend::Namespace nspace;

        SharedVolumePtr v = newVolume(volname,
                              nspace);

        BackendInterfacePtr bi(v->getBackendInterface()->cloneWithNewNamespace(nspace));
        SCOAccessDataPersistor sadp(std::move(bi));

        if (must_exist)
        {
            SCOAccessData sad(nspace);
            EXPECT_THROW(sadp.pull(sad, must_exist),
                         BackendObjectDoesNotExistException);
        }
        else
        {
            SCOAccessData sad(nspace);
            EXPECT_NO_THROW(sadp.pull(sad));
            EXPECT_EQ(nspace, sad.getNamespace());
            EXPECT_EQ(0U, sad.getVector().size());
        }
    }
};

TEST_P(SCOAccessDataTest, test1)
{
    const std::string volname("volume1");
    // const backend::Namespace nspace;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& nspace = ns_ptr->ns();

    SharedVolumePtr v1 = newVolume(volname,
                           nspace);

    SCOAccessDataPersistor sadp(v1->getBackendInterface()->cloneWithNewNamespace(nspace));

    {
        std::unique_ptr<SCOAccessData> sad;
        ASSERT_NO_THROW(sad = makeSCOAccessData(*v1));
        EXPECT_EQ(nspace, sad->getNamespace());
    }

    {
        SCOAccessData sad(nspace);
        EXPECT_NO_THROW(sadp.pull(sad));
        EXPECT_FALSE(v1->getBackendInterface()->objectExists(SCOAccessDataPersistor::backend_name));
        EXPECT_EQ(nspace, sad.getNamespace());
    }

    {
        std::unique_ptr<SCOAccessData> sad(sadp.pull());
        EXPECT_FALSE(v1->getBackendInterface()->objectExists(SCOAccessDataPersistor::backend_name));

        EXPECT_EQ(nspace, sad->getNamespace());
    }

    destroyVolume(v1,
                  DeleteLocalData::T,
                  RemoveVolumeCompletely::F);
}

TEST_P(SCOAccessDataTest, test1_5)
{
    createAndPutSCOAccessData(1);
}

TEST_P(SCOAccessDataTest, test2)
{
    createAndPutSCOAccessData(1ULL << 15);
}

TEST_P(SCOAccessDataTest, DISABLED_CreateOne)
{
    const std::string volname("volume1");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& nspace = ns_ptr->ns();

    // const backend::Namespace nspace;

    SharedVolumePtr v1 = newVolume(volname,
                           nspace);


    std::unique_ptr<SCOAccessData> test(makeSCOAccessData(*v1));

    test->addData(SCO(1),
                 12.3);
    test->addData(SCO(2),
                 14.3);

    std::stringstream oss;
    Serialization::serializeAndFlush<SCOAccessDataPersistor::oarchive_type>(oss,
                                                                            *test);
    int i = 0;

    BOOST_FOREACH(char c, oss.str())
    {
        std::cout << static_cast<int>(c) << (((++i % 10) == 0) ? "\n" : ", ");
    }
}

TEST_P(SCOAccessDataTest, pullInexistent1)
{
    pullInexistentPtr(false);
}

TEST_P(SCOAccessDataTest, pullInexistent2)
{
    pullInexistentPtr(true);
}

TEST_P(SCOAccessDataTest, pullInexistent3)
{
    pullInexistentRef(false);
}

TEST_P(SCOAccessDataTest, pullInexistent4)
{
    pullInexistentRef(true);
}

TEST_P(SCOAccessDataTest, InAndOut)
{
    const std::string volname("volume");
    // const backend::Namespace nspace;
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& nspace = ns_ptr->ns();

    SharedVolumePtr v = newVolume(volname,
                          nspace);

    const size_t size = 1024;
    const float readactivity = 0.123;

    SCOAccessData sad_in(nspace, readactivity);

    sad_in.reserve(size);

    for (size_t i = 0; i < size; ++i)
    {
        sad_in.addData(SCO(i), 0.0);
    }

    EXPECT_EQ(nspace, sad_in.getNamespace());
    EXPECT_EQ(readactivity, sad_in.read_activity());
    EXPECT_EQ(size, sad_in.getVector().size());

    EXPECT_FALSE(v->getBackendInterface()->objectExists(SCOAccessDataPersistor::backend_name));

    BackendInterfacePtr bi(v->getBackendInterface()->cloneWithNewNamespace(nspace));
    SCOAccessDataPersistor sadp(std::move(bi));

    sadp.push(sad_in,
              v->backend_write_condition());

    EXPECT_TRUE(v->getBackendInterface()->objectExists(SCOAccessDataPersistor::backend_name));

    SCOAccessDataPtr sad_out(sadp.pull());

    EXPECT_EQ(sad_in.getNamespace(), sad_out->getNamespace());
    EXPECT_EQ(sad_in.read_activity(), sad_out->read_activity());
    EXPECT_TRUE(sad_in.getVector() == sad_out->getVector());
}

TEST_P(SCOAccessDataTest, rebase)
{
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& parent_ns = ns_ptr->ns();

    // const backend::Namespace parent_ns;

    const size_t size = 1024;

    const float read_activity = 0.667;

    SCOAccessData ref_sad(parent_ns,
                          read_activity);

    SCOAccessData sad(parent_ns,
                      read_activity);

    ref_sad.reserve(size);
    sad.reserve(size);

    for (size_t i = 0; i < size; ++i)
    {
        const float rand = drand48();
        ref_sad.addData(SCO(i), rand);
        sad.addData(SCO(i), rand);
    }

    auto ns2_ptr = make_random_namespace();

    const backend::Namespace& clone_ns = ns2_ptr->ns();

    //    const backend::Namespace clone_ns;
    sad.rebase(clone_ns);

    EXPECT_EQ(clone_ns, sad.getNamespace());
    EXPECT_EQ(read_activity, sad.read_activity());
    EXPECT_EQ(size, sad.getVector().size());

    const SCOAccessData::VectorType& ref_vec = ref_sad.getVector();
    const SCOAccessData::VectorType& vec = sad.getVector();

    for (size_t i = 0; i < size; ++i)
    {
        EXPECT_EQ(ref_vec[i].second, vec[i].second);

        const SCO& ref_sco = ref_vec[i].first;
        const SCO& sco = vec[i].first;

        EXPECT_EQ(static_cast<int>(ref_sco.cloneID()) + 1,
                  static_cast<int>(sco.cloneID()));
    }
}

TEST_P(SCOAccessDataTest, CruelAndUnusualPunishment)
{
    const std::string volname("volume1");
    auto ns_ptr = make_random_namespace();

    const backend::Namespace& nspace = ns_ptr->ns();

    SharedVolumePtr v1 = newVolume(volname,
                           nspace);

    std::unique_ptr<SCOAccessData> test(makeSCOAccessData(*v1));
    (void) test;

    /*
    test.addData(SCO::createSCOName(0,1,0),
             INFINITY);
    test.addData(SCO::createSCOName(0,2,0),
             NAN);

    fs::path file = FileUtils::create_temp_file("/tmp",
                                                "sco_access_data_for_test");
    {
        fs::ofstream ofs(file);
        SCOAccessData::oarchive_type oa(ofs);
        oa << test;
        ofs.close();
    }

    {
        SCOAccessData test2(file);
        const SCOAccessData::VectorType& vec = test2.getVector();
        EXPECT_EQ(vec.size(), 2);

        for(SCOAccessData::VectorType::const_iterator it = vec.begin();
            it != vec.end();
            ++it)
        {

            EXPECT_TRUE(isnan(it->second) or
                        isinf(it->second));

        }

    }


    destroyVolume(v1, true, true);
    */

}

INSTANTIATE_TEST(SCOAccessDataTest);
}

// Local Variables: **
// mode: c++ **
// End: **
