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

#ifndef STRONG_TYPED_STRING_H_
#define STRONG_TYPED_STRING_H_

#include <functional>
#include <ostream>
#include <string>

#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/nvp.hpp>

// This has a lot in common with OUR_STRONG_TYPEDEF (BOOST_STRONG_TYPEDEF,
// actually) - unify?

#define STRONG_TYPED_STRING(ns, name)                                   \
    namespace ns                                                        \
    {                                                                   \
    class name : public std::string                                     \
    {                                                                   \
        public:                                                         \
        explicit name()                                                 \
        {}                                                              \
        explicit name(const char* str)                                  \
            : std::string(str)                                          \
        {};                                                             \
                                                                        \
        explicit name(const std::string& str)                           \
            : std::string(str)                                          \
        {}                                                              \
                                                                        \
        explicit name(const std::string&& str)                          \
            : std::string(std::move(str))                               \
        {}                                                              \
                                                                        \
        const std::string&                                              \
        str() const                                                     \
        {                                                               \
            return *this;                                               \
        }                                                               \
                                                                        \
        std::string&                                                    \
        str()                                                           \
        {                                                               \
            return *this;                                               \
        }                                                               \
                                                                        \
        bool                                                            \
        operator==(const name& other) const                             \
        {                                                               \
            return str() == other.str();                                \
        }                                                               \
                                                                        \
        friend std::ostream&                                            \
        operator<<(std::ostream& os,                                    \
               const name& in)                                          \
        {                                                               \
            return (os << in.str());                                    \
        }                                                               \
                                                                        \
    private:                                                            \
        friend class boost::serialization::access;                      \
        BOOST_SERIALIZATION_SPLIT_MEMBER();                             \
                                                                        \
        template<class Archive>                                         \
        void                                                            \
        load(Archive& ar, const unsigned int /*version*/)               \
        {                                                               \
            ar & boost::serialization::make_nvp(#name,                  \
                                                static_cast<std::string&>(*this)); \
        }                                                               \
                                                                        \
        template<class Archive>                                         \
            void                                                        \
            save(Archive& ar, const unsigned int /*version*/) const     \
        {                                                               \
            ar & boost::serialization::make_nvp(#name, \
                                                static_cast<const std::string&>(*this)); \
        }                                                               \
    };                                                                  \
                                                                        \
    }                                                                   \
                                                                        \
    namespace std                                                       \
    {                                                                   \
        template<>                                                      \
        class hash<ns::name>                                            \
        {                                                               \
        public:                                                         \
            size_t                                                      \
            operator()(const ns::name& n) const                         \
            {                                                           \
                return std::hash<string>()(static_cast<const string&>(n)); \
            }                                                           \
        };                                                              \
    }

#endif // STRONG_TYPED_STRING_H_

// Local Variables: **
// mode: c++ **
// End: **
