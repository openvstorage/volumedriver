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

#ifndef SCOACCESS_DATA_H
#define SCOACCESS_DATA_H

#include <string>
#include <youtils/Logging.h>
#include "Types.h"
#include "youtils/Serialization.h"
#include "backend/BackendInterface.h"
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include "SCO.h"
#include <youtils/Assert.h>

namespace volumedriver
{
class Volume;
class SCOAccessDataTest;
class VolManagerTestSetup;
using ::youtils::Serialization;

// Z42: nspace_ is not strictly required - I'll leave it in for debugging purposes
class SCOAccessData
{
public:
    typedef std::pair<SCO, float> EntryType;
    typedef std::vector<EntryType> VectorType;

    explicit SCOAccessData(const backend::Namespace& nspace,
                           float read_activity = 0);

    explicit SCOAccessData(float read_activity = 0);

    void
    addData(SCO name,
            float data);

    const VectorType&
    getVector() const;

    float
    read_activity() const;

    void
    rebase(const Namespace& nspace);

    const Namespace&
    getNamespace() const;

    void
    reserve(size_t reserve);

private:
    DECLARE_LOGGER("SCOAccessData");

    std::unique_ptr<backend::Namespace> nspace_;

    float read_activity_;
    std::vector<std::pair<SCO, float> > scos_;

    friend class boost::serialization::access;

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        if(version == 0)
        {
            LOG_INFO("Loading an old SCOAccessData will be empty");
            std::string str;
            ar & str;
            nspace_.reset(new backend::Namespace(str));

            ar & read_activity_;

        }
        if(version == 1)
        {

            std::string str;
            ar & str;
            nspace_.reset(new backend::Namespace(str));

            ar & read_activity_;
            ar & scos_;
        }

    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned int version) const
    {
        VERIFY(version == 1);
        ar & getNamespace().str();
        ar & read_activity_;
        ar & scos_;
    }
};

typedef std::unique_ptr<SCOAccessData> SCOAccessDataPtr;

typedef std::list<SCOAccessData*> SCOAccessDataList;

class SCOAccessDataPersistor
{
public:
    // Y42 I dare you to change one without changing the other.
    typedef boost::archive::binary_iarchive iarchive_type;
    typedef boost::archive::binary_oarchive oarchive_type;

    static const char* backend_name;

    explicit SCOAccessDataPersistor(BackendInterfacePtr bi);

    SCOAccessDataPersistor(const SCOAccessDataPersistor&) = delete;

    SCOAccessDataPersistor&
    operator=(SCOAccessDataPersistor&) = delete;

    void
    pull(SCOAccessData&,
         bool must_exist = false);

    SCOAccessDataPtr
    pull(bool must_exist = false);

    void
    push(const SCOAccessData&,
         const boost::shared_ptr<backend::Condition>&);

    static SCOAccessDataPtr
    deserialize(const fs::path& p);

private:
    DECLARE_LOGGER("SCOAccessDataPersistor");
    BackendInterfacePtr bi_;

    // exposing these migh be handy at some point.
    static void
    serialize(const fs::path& p,
              const SCOAccessData& sad);

    static void
    deserialize(const fs::path& p,
                SCOAccessData& sad);
};

}

BOOST_CLASS_VERSION(volumedriver::SCOAccessData,1);

#endif //SCOACCESS_DATA_H

// Local Variables: **
// mode: c++ **
// End: **
