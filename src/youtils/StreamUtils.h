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

#ifndef YOUTILS_STREAM_UTILS_H_
#define YOUTILS_STREAM_UTILS_H_

#include "Assert.h"
#include "Logging.h"

#include <iosfwd>
#include <type_traits>

namespace youtils
{

struct StreamUtils
{
    // Helpers for translations that are kept in a map-like container.
    template<typename M>
    static std::ostream&
    stream_out(const M& m,
               std::ostream& os,
               const typename M::key_type& t)
    {
        const auto it = m.find(t);
        if (it != m.end())
        {
            os << it->second;
        }
        else
        {
            ASSERT(0 == "missing translation to string!?");
            os.setstate(std::ios_base::failbit);
        }

        return os;
    }

    template<typename M>
    static std::istream&
    stream_in(const M& m,
              std::istream& is,
              std::add_lvalue_reference_t<std::remove_const_t<typename M::mapped_type>> t)
    {
        std::string s;
        is >> s;

        const auto it = m.find(s);
        if (it != m.end())
        {
            t = it->second;
        }
        else
        {
            LOG_ERROR("Cannot parse \"" << s << "\"");
            is.setstate(std::ios_base::failbit);
        }

        return is;
    }

    // AR: add sanity checks of S so we get helpful compiler messages.
    template<typename S>
    static std::ostream&
    stream_out_sequence(std::ostream& os,
                        const S& seq)
    {
        bool sep = false;

        os << "[";

        for (const auto& s : seq)
        {
            if (sep)
            {
                os << ", ";
            }
            else
            {
                sep = true;
            }

            os << s;
        }

        os << "]";

        return os;
    }

    DECLARE_LOGGER("StreamUtils");
};

}

#endif // !YOUTILS_STREAM_UTILS_H_
