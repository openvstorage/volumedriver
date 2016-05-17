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

#ifndef STRONG_TYPED_PATH_H_
#define STRONG_TYPED_PATH_H_

#include "Serialization.h"

#include <functional>
#include <ostream>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/nvp.hpp>

// This has a lot in common with STRONG_TYPED_STRING
// actually) - unify?

#define STRONG_TYPED_PATH(ns, name)                                     \
    namespace ns                                                        \
    {                                                                   \
    class name : public boost::filesystem::path                         \
    {                                                                   \
        public:                                                         \
        explicit name()                                                 \
        {}                                                              \
        explicit name(const char* str)                                  \
           : boost::filesystem::path(str)                               \
        {};                                                             \
                                                                        \
        explicit name(const boost::filesystem::path& str)               \
            : boost::filesystem::path(str)                              \
        {}                                                              \
                                                                        \
        explicit name(const boost::filesystem::path&& str)              \
            : boost::filesystem::path(std::move(str))                   \
        {}                                                              \
                                                                        \
        const boost::filesystem::path&                                  \
        path() const                                                    \
        {                                                               \
            return *this;                                               \
        }                                                               \
                                                                        \
        boost::filesystem::path&                                        \
        path()                                                          \
        {                                                               \
            return *this;                                               \
        }                                                               \
                                                                        \
        const std::string&                                              \
        str() const                                                     \
        {                                                               \
            return this->string();                                      \
        }                                                               \
                                                                        \
        bool                                                            \
        operator==(const name& other) const                             \
        {                                                               \
            return path() == other.path();                              \
        }                                                               \
                                                                        \
        friend std::ostream&                                            \
        operator<<(std::ostream& os,                                    \
               const name& in)                                          \
        {                                                               \
            return (os << in.path());                                   \
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
            std::string tmp;                                            \
            ar >> boost::serialization::make_nvp(#name,                 \
                                                 tmp);                  \
            *this = name(tmp);                                          \
        }                                                               \
                                                                        \
        template<class Archive>                                         \
            void                                                        \
            save(Archive& ar, const unsigned int /*version*/) const     \
        {                                                               \
            ar << boost::serialization::make_nvp(#name,                 \
                                                 str());                \
        }                                                               \
    };                                                                  \
                                                                        \
    }                                                                   \
                                                                        \
    BOOST_CLASS_VERSION(ns::name, 1);                                   \
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
                return std::hash<std::string>()(n.str());               \
            }                                                           \
        };                                                              \
    }

#endif // STRONG_TYPED_PATH_H_

// Local Variables: **
// mode: c++ **
// End: **
