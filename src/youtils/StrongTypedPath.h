// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
