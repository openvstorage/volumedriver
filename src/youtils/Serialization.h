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

#ifndef _SERIALIZATION_H_
#define _SERIALIZATION_H_

#include "Logging.h"

#include <atomic>
#include <exception>
#include <map>
#include <sstream>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/serialization/nvp.hpp>

namespace youtils
{

class SerializationException
    : public std::exception
{
public:
    SerializationException()
    {};
};

class SerializationFlushException
    : public SerializationException
{
public:
    explicit SerializationFlushException(const std::string& name)
        : message_(name + ": Serialization error - failed to flush stream")
    {}

    SerializationFlushException()
        : message_("Serialization error - failed to flush stream")
    {}

    virtual
    ~SerializationFlushException() throw ()
    {}

    virtual const char*
    what() const throw ()
    {
        return message_.c_str();
    }

private:
    const std::string message_;
};

#define THROW_SERIALIZATION_ERROR(version, max, min) \
    throw youtils::SerializationVersionException(__PRETTY_FUNCTION__,version,max,min);

#define CHECK_VERSION(actual_version, expected_version)                                      \
    if(actual_version != expected_version)                                                   \
    {                                                                                        \
        THROW_SERIALIZATION_ERROR(actual_version, expected_version, expected_version);       \
    }                                                                                        \

class SerializationVersionException
    : public SerializationException
{
public:
    SerializationVersionException(const std::string& place,
                                  const uint version,
                                  const unsigned int max_expected = 0,
                                  const unsigned int min_expected = 0)
        // : version_(version)
        // , max_(max_expected)
        // , min_(min_expected)
    {
        std::stringstream ss;
        ss<< place << ": Got version " << version << " Expected between " << max_expected << " and " << min_expected;
        message = ss.str();
        msg = message.c_str();
    }

    virtual
    ~SerializationVersionException() throw ()
    {}

    virtual const char* what() const throw()
    {
        return msg;

    }

private:
    // const unsigned int version_;
    // const unsigned int max_;
    // const unsigned int min_;
    std::string message;
    const char* msg;

};

struct Serialization
{
    Serialization() = delete;

    ~Serialization() = delete;

    template<typename OArchiveT, typename T>
    static void
    serializeNVPAndFlush(std::ostream& os, const std::string& name, const T& obj)
    {
        OArchiveT oa(os);

        // throws in case of errors - no need to check for os.good() afterwards
        oa << boost::serialization::make_nvp(name.c_str(), obj);
        os.flush();
        if (not os.good())
        {
            LOG_ERROR("Error serializing " << name <<
                      ": failed to flush output stream");
            throw SerializationFlushException(name);
        }
    }

    template<typename OArchiveT, typename T>
    static void
    serializeNVPAndFlush(const boost::filesystem::path& p, const std::string& name, const T& obj)
    {
        boost::filesystem::ofstream ofs(p);
        serializeNVPAndFlush<OArchiveT>(ofs, name, obj);
        ofs.close();
    }

    template<typename OArchiveT, typename T>
    static void
    serializeAndFlush(std::ostream& os, const T& obj)
    {
        OArchiveT oa(os);
        oa << obj;
        os.flush();
        if (not os.good())
        {
            LOG_ERROR("Error serializing: failed to flush output stream");
            throw SerializationFlushException();
        }
    }

    template<typename OArchiveT, typename T>
    static void
    serializeAndFlush(const boost::filesystem::path& p, const T& obj)
    {
        boost::filesystem::ofstream ofs(p);
        serializeAndFlush<OArchiveT>(ofs, obj);
        ofs.close();
    }

    DECLARE_LOGGER("Serialization");
};

}

namespace boost {

namespace serialization {

template<class Archive>
void serialize(Archive& ar,
               boost::filesystem::path& p,
               const unsigned int /* version */)
{
    std::string s(p.string());
    ar & s;
    p = boost::filesystem::path(s);
}

template<typename Archive, typename T>
void
save(Archive& ar,
     const std::atomic<T>& t,
     const int unsigned /* version */)
{
    const T value = t.load();
    ar << BOOST_SERIALIZATION_NVP(value);
}

template<typename Archive, typename T>
void
load(Archive& ar,
     std::atomic<T>& t,
     const int unsigned /* version */)
{
    T value;
    ar >> BOOST_SERIALIZATION_NVP(value);
    t = value;
}

template<typename A, typename T>
inline void
serialize(A& a,
          std::atomic<T>& t,
          const unsigned version)
{
    split_free(a, t, version);
}

template<typename A,
         typename K,
         typename V>
void
save(A& ar,
     const std::map<K, std::unique_ptr<V>>& map,
     const unsigned /* version */)
{
    const uint64_t size = map.size();
    ar & size;
    for (const auto& v : map)
    {
        ar & v.first;
        ar & *v.second;
    }
}

template<typename A,
         typename K,
         typename V>
void
load(A& ar,
     std::map<K, std::unique_ptr<V>>& map,
     const unsigned /* version */)
{
    uint64_t size;
    ar & size;

    for (uint64_t i = 0; i < size; ++i)
    {
        K k;
        ar & k;
        auto v(std::make_unique<V>());
        ar & *v;

        map.emplace(k,
                    std::move(v));
    }
}

template<typename A,
         typename K,
         typename V>
void
serialize(A& ar,
          std::map<K, std::unique_ptr<V>>& map,
          const unsigned version)
{
    split_free(ar,
               map,
               version);
}


} //serialization namespace
} // boost namespace

#endif // _SERIALIZATION_H_

// Local Variables: **
// mode: c++ **
// End: **
