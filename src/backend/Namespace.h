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

#ifndef NAMESPACE_H
#define NAMESPACE_H

#include <string.h>
#include <iosfwd>

#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>

#include <youtils/Assert.h>
#include <youtils/IOException.h>
#include <youtils/Logging.h>
#include <youtils/Serialization.h>
#include <boost/functional/hash.hpp>

namespace scrubbing
{

class ScrubWork;

}

namespace backend
{


class NamespaceSerializationWrapper;

class Namespace
{
    friend class NamespaceSerializationWrapper;
    friend class ScrubWork;

public:
    MAKE_EXCEPTION(Exception, fungi::IOException)
    MAKE_EXCEPTION(TooShortException, Namespace::Exception)
    MAKE_EXCEPTION(TooLongException, Namespace::Exception)
    MAKE_EXCEPTION(LabelTooShortException, Namespace::Exception)
    MAKE_EXCEPTION(LabelStartException, Namespace::Exception)
    MAKE_EXCEPTION(LabelEndException, Namespace::Exception)
    MAKE_EXCEPTION(LabelNameException, Namespace::Exception)

    explicit Namespace(const std::string&);

    // Creates a namespace with a random UUID.
    Namespace();

    Namespace(const char*) = delete;

    Namespace(const Namespace&);

    Namespace(std::string&&);

    Namespace(Namespace&&);

    // These seem superflous
    Namespace&
    operator=(const Namespace&);

    Namespace&
    operator=(Namespace&&);

    // explicit operator const std::string& () const;

    const char*
    c_str() const;

    bool
    operator!=(const Namespace& n2) const
    {
        return str_ != n2.str_;
    }

    bool
    operator<(const Namespace& n2) const
    {
        return str_ < n2.str_;
    }

    const std::string&
    str() const
    {
        return str_;
    }

    bool
    operator==(const Namespace& ns2) const
    {
        return str_ == ns2.str_;
    }

    friend class boost::serialization::access;

    template<class Archive>
    void
    serialize(Archive& ar, const unsigned int /*version*/)
    {
        ar & boost::serialization::make_nvp(nvp_name.c_str(),
                                            const_cast<std::string&>(str_));
    }

    const static std::string nvp_name;



private:
    DECLARE_LOGGER("Namespace");

    std::string str_;
};

inline size_t
hash_value(const Namespace& ns)
{
    return ::boost::hash_value(ns.str());
}


inline std::ostream&
operator<<(std::ostream& ostr,
           const backend::Namespace& in)
{
    return (ostr << in.str());
}


class NamespaceDeSerializationHelper
{
public:

    NamespaceDeSerializationHelper()
    {};

    NamespaceDeSerializationHelper(const NamespaceDeSerializationHelper&) = delete;

    NamespaceDeSerializationHelper&
    operator=(const NamespaceDeSerializationHelper&) = delete;

    NamespaceDeSerializationHelper(NamespaceDeSerializationHelper&& other) = delete;


    DECLARE_LOGGER("NamespaceDeSerializationHelper");

    std::unique_ptr<backend::Namespace>
    getNS()
    {
        return std::unique_ptr<backend::Namespace>(new backend::Namespace(ns_));
    }


private:
    friend class boost::serialization::access;
    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar,
         const unsigned int /*version*/)
    {
        ar & boost::serialization::make_nvp(Namespace::nvp_name.c_str(),
                                           ns_);
    }

private:
    std::string ns_;
}
;


}

#endif // NAMESPACE_H
