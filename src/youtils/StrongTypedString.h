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
