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

#ifndef YT_ALTERNATIVE_OPTION_AGAIN_H_
#define YT_ALTERNATIVE_OPTION_AGAIN_H_
#include <string>
#include <vector>
#include <loki/Typelist.h>

#include <memory>
#include <ostream>

namespace boost
{
namespace program_options
{
class options_description;
}
}

namespace youtils
{
template<typename T>
struct AlternativeOptionTraits;

template<typename T>
struct AltOptionInterface
{
    typedef T option_type;

    virtual ~AltOptionInterface() = default;

    virtual void
    actions() = 0;

    virtual const boost::program_options::options_description&
    options_description() const = 0;

};



template<typename T,
         T value>
struct AlternativeOptionAgain : public AltOptionInterface<T>
{
    explicit AlternativeOptionAgain()
    {
        static_assert(std::is_enum<T>::value,"Only works with enums");
    }

    AlternativeOptionAgain(const AlternativeOptionAgain&) = delete;

    AlternativeOptionAgain&
    operator=(const AlternativeOptionAgain&) = delete;

    static bool
    is_ok(const std::string& in)
    {
        return AlternativeOptionTraits<T>::str_.at(value) == in;
    }

    static const char*
    id_()
    {
        return AlternativeOptionTraits<T>::str_.at(value);
    }

};

template<typename T>
struct make_alt_option_interface;


template<typename Options, Options option>
struct make_alt_option_interface<AlternativeOptionAgain<Options, option> >
{
    typedef AlternativeOptionAgain<Options, option> option_type;
};


template <class TList>
struct print_doc;

template<typename Head, typename Tail>
struct print_doc< ::Loki::Typelist<Head, Tail> >
{
    static void
    go(std::ostream& strm)
    {
        strm << Head().options_description() << std::endl;
        print_doc<Tail>::go(strm);
    }
};

template<>
struct print_doc< ::Loki::NullType >
{
    static void
    go(std::ostream& /*strm*/)
    {
    }
};



template <class TList>
struct GetOption;

template<typename Head, typename Tail>
struct GetOption< ::Loki::Typelist<Head, Tail> >
{
    static void*
    get(const std::string& arg)
    {
        if (Head::is_ok(arg))
        {
            return new Head();
        }
        else
        {
            return GetOption<Tail>::get(arg);
        }
    }
};


template<>
struct GetOption<  ::Loki::NullType >
{
    static void*
    get(const std::string& /*arg*/)
    {
        return nullptr;
    }
};

template<typename T,
         typename List>
std::unique_ptr<AltOptionInterface<T> >
get_option(const std::string& str)
{
    std::unique_ptr<AltOptionInterface<T > > val
        (reinterpret_cast<AltOptionInterface<T> *>(GetOption<List>::get(str)));
    if(not val)
    {
        throw std::out_of_range("No such option" + str);
    }
    else
    {
        return val;
    }
}

}


#endif // YT_ALTERNATIVE_OPTION_AGAIN_H_
