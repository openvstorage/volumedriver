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

#ifndef VD_BOOLEAN_ENUM_H_
#define VD_BOOLEAN_ENUM_H_

// <iostream> is not included here on purpose, as this header is also used by
// Logger.h which in turn is used by Logging.h.
#include <iostream>
#define DECLARE_VD_BOOLEAN_ENUM(name) enum class name : bool;

#define VD_BOOLEAN_ENUM(name)                                              \
    enum class name : bool                                              \
                                                                        \
    {                                                                   \
        F = false,                                                      \
            T = true                                                    \
            };                                                          \
                                                                        \
    inline std::ostream&                                                \
    operator<<(std::ostream& os,                                        \
               const name& in)                                          \
    {                                                                   \
        return os << (in == name::T ? (#name "::T") : (#name "::F"));   \
    }                                                                   \
                                                                        \
    inline bool                                                         \
    T(const name& in)                                                   \
    {                                                                   \
        return in == name::T;                                           \
    }                                                                   \
                                                                        \
    inline bool                                                         \
    BooleanEnumTrue(const name& in)                                     \
    {                                                                   \
        return in == name::T;                                           \
    }                                                                   \
                                                                        \
    inline bool                                                         \
    F(const name& in)                                                   \
    {                                                                   \
        return in == name::F;                                           \
    }                                                                   \
                                                                        \
    inline bool                                                         \
    BooleanEnumFalse(const name& in)                                    \
    {                                                                   \
        return in == name::F;                                           \
    }                                                                   \
                                                                        \
    inline std::istream&                                                \
    operator>>(std::istream& is,                                        \
               name& out)                                               \
    {                                                                   \
        static const std::string e_name(#name "::");                    \
        char c;                                                         \
        for(size_t i = 0; i< e_name.size(); ++i)                        \
        {                                                               \
            is >> c;                                                    \
            if(not is or                                                \
               c != e_name[i])                                          \
            {                                                           \
                is.setstate(std::ios_base::badbit);                     \
                return is;                                              \
            }                                                           \
        }                                                               \
        is >> c;                                                        \
        if(not is)                                                      \
        {                                                               \
            is.setstate(std::ios_base::badbit);                         \
            return is;                                                  \
        }                                                               \
        switch(c)                                                       \
        {                                                               \
        case 'T':                                                       \
            out = name ::T;                                             \
            return is;                                                  \
        case 'F':                                                       \
            out = name ::F;                                             \
            return is;                                                  \
        default:                                                        \
            is.setstate(std::ios_base::badbit);                         \
            return is;                                                  \
        }                                                               \
    }

#endif // BOOLEAN_H_

// Local Variables: **
// mode: c++ **
// End: **
