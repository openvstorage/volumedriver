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

#ifndef VFS_PICCALILLI_H_
#define VFS_PICCALILLI_H_

#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python/class.hpp>
#include <boost/python/tuple.hpp>
#include <filesystem/XMLRPCStructs.h>
#include <string>

template<typename T>
struct PickleTraits
{
    static std::string
    str(const T& t)
    {
        return t.str();
    }

    static void
    unstr(T& t,
          const std::string& s)
    {
        t.set_from_str(s);
    }
};

using PerformanceCounterU64 = volumedriver::PerformanceCounter<uint64_t>;

template<>
struct PickleTraits<volumedriver::ClusterCacheHandle>
{
    using T = volumedriver::ClusterCacheHandle;

    static std::string
    str(const T& t)
    {
        return boost::lexical_cast<std::string>(t);
    }

    static void
    unstr(T& t,
          const std::string& s)
    {
        t = boost::lexical_cast<T>(s);
    }
};

template<>
struct PickleTraits<volumedriver::OwnerTag>
{
    using T = volumedriver::OwnerTag;

    static std::string
    str(const T& t)
    {
        return boost::lexical_cast<std::string>(t);
    }

    static void
    unstr(T& t,
          const std::string& s)
    {
        t = boost::lexical_cast<T>(s);
    }
};

template<>
struct PickleTraits<PerformanceCounterU64>
{
    using IArchive = boost::archive::xml_iarchive;
    using OArchive = boost::archive::xml_oarchive;

    static std::string
    str(const PerformanceCounterU64& c)
    {
        std::stringstream ss;
        OArchive oa(ss);
        oa << boost::serialization::make_nvp("PerformanceCounterU64",
                                             c);
        return ss.str();
    }

    static void
    unstr(PerformanceCounterU64& c,
          const std::string& s)
    {
        std::stringstream ss(s);
        IArchive ia(ss);
        ia >> boost::serialization::make_nvp("PerformanceCounterU64",
                                             c);
    }
};

template<>
struct PickleTraits<volumedriver::PerformanceCounters>
{
    using IArchive = boost::archive::xml_iarchive;
    using OArchive = boost::archive::xml_oarchive;

    static std::string
    str(const volumedriver::PerformanceCounters& c)
    {
        std::stringstream ss;
        OArchive oa(ss);
        oa << boost::serialization::make_nvp("PerformanceCounters",
                                             c);
        return ss.str();
    }

    static void
    unstr(volumedriver::PerformanceCounters& c,
          const std::string& s)
    {
        std::stringstream ss(s);
        IArchive ia(ss);
        ia >> boost::serialization::make_nvp("PerformanceCounters",
                                             c);
    }
};

template<typename T,
         typename Traits = PickleTraits<T>>
struct PickleSuite
    : boost::python::pickle_suite
{
    static boost::python::tuple
    getinitargs(const T&)
    {
        return boost::python::make_tuple();
    }

    static boost::python::tuple
    getstate(const T& t)
    {
        return boost::python::make_tuple(Traits::str(t));
    }

    static void
    setstate(T& t,
             boost::python::tuple state)
    {
        using namespace boost::python;
        if(len(state) != 1)
        {
            PyErr_SetObject(PyExc_ValueError,
                            ("expected 1-item tuple in call to __setstate__; got %s"
                             % state).ptr()
                            );
            throw_error_already_set();
        }

        Traits::unstr(t,
                      extract<std::string>(state[0]));
    }
};

using XMLRPCVolumeInfoPickleSuite = PickleSuite<volumedriverfs::XMLRPCVolumeInfo>;
using XMLRPCClusterCacheHandleInfoPickleSuite =
    PickleSuite<volumedriverfs::XMLRPCClusterCacheHandleInfo>;
using XMLRPCStatisticsPickleSuite = PickleSuite<volumedriverfs::XMLRPCStatistics>;
using XMLRPCSnapshotInfoPickleSuite = PickleSuite<volumedriverfs::XMLRPCSnapshotInfo>;
using PerformanceCounterU64PickleSuite =
    PickleSuite<volumedriver::PerformanceCounter<uint64_t>>;
using PerformanceCountersPickleSuite = PickleSuite<volumedriver::PerformanceCounters>;
using ClusterCacheHandlePickleSuite = PickleSuite<volumedriver::ClusterCacheHandle>;
using OwnerTagPickleSuite = PickleSuite<volumedriver::OwnerTag>;

#endif //VFS_PICCALILLI_H_
