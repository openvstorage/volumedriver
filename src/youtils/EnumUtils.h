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

#ifndef YT_ENUM_UTILS_H
#define YT_ENUM_UTILS_H

#include <exception>
#include <string>

namespace youtils
{

template<typename T>
class EnumParseError
    : public std::exception
{
public:
    EnumParseError(const std::string& str) throw()
        : str_(str)
        , message_(str)
    {
        message_ += " is not one of ";
        for(unsigned i = 0; i < T::size; ++i)
        {
            message_ +=  T::strings[i];
            message_ += " ";
        }
    }

    ~EnumParseError() throw()
    {}

    virtual const char* what() const throw()
    {
        return message_.c_str();
    }
private:
    std::string str_;
    std::string message_;
};

template<typename Enum>
struct EnumTraits;

struct EnumUtils
{
    template<typename Enum>
    static Enum
    fromInteger(unsigned i)
    {
        return static_cast<Enum>(i);
    }

    template<typename Enum,
             typename Traits = EnumTraits<Enum>>
    static std::istream&
    read_from_stream(std::istream& is,
                     Enum& jt)
    {
        std::string val;
        is >> val;
        for(unsigned i = 0; i < sizeof(Traits::strings); ++i)
        {
            if(val == Traits::strings[i])
            {
                jt = fromInteger<Enum>(i);
                return is;
            }
        }
        throw EnumParseError<Traits>(val);
    }

    template<typename Enum,
             typename Traits = EnumTraits<Enum>>
    static std::ostream&
    write_to_stream(std::ostream& os,
                    const Enum& enum_)
    {
        for(unsigned i = 0; i < sizeof(Traits::strings); ++i)
        {
            if(fromInteger<Enum>(i) == enum_)
            {
                std::string s(Traits::strings[i]);
                return (os << s);
            }
        }
        throw "something";
    }
};

}

#endif // !YT_ENUM_UTILS_H
