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

#ifndef _SERIALIZATION_H_
#define _SERIALIZATION_H_

#include <atomic>
#include <exception>
#include <map>
#include <sstream>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "Logging.h"

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
    ar << value;
}

template<typename Archive, typename T>
void
load(Archive& ar,
     std::atomic<T>& t,
     const int unsigned /* version */)
{
    T value;
    ar >> value;
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
