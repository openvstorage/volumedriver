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
