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

#ifndef OUR_STRONG_TYPEDEF_H_
#define OUR_STRONG_TYPEDEF_H_

// There are 2 flavours of BOOST_STRONG_TYPEDEF - the one we use below,
// and one in boost/serialization/strong_typedef.hpp, which is
// slightly different. Since the latter one is also dragged in by
// boost/serialization/serialization.hpp we use that one to avoid clashes.
#include <limits>

#include <boost/serialization/nvp.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/strong_typedef.hpp>
#include <boost/type_traits/is_arithmetic.hpp>

#define OUR_STRONG_TYPEDEF_COMMON(type, name, ns)                       \
    namespace ns                                                        \
    {                                                                   \
    BOOST_STRONG_TYPEDEF(type, name);                                   \
                                                                        \
    inline std::ostream&                                                \
    operator<<(std::ostream& os,                                        \
               const name& n)                                           \
    {                                                                   \
        os << static_cast<const type&>(n);                              \
        return os;                                                      \
    }                                                                   \
    }                                                                   \
                                                                        \
    namespace boost                                                     \
    {                                                                   \
    namespace serialization                                             \
    {                                                                   \
    template<class Archive>                                             \
    void                                                                \
    serialize(Archive& ar, ns::name& n, const unsigned /*version */)    \
    {                                                                   \
        ar & boost::serialization::make_nvp(#name,                      \
                                            static_cast<type&>(n));     \
    }                                                                   \
    }                                                                   \
    }

#define OUR_STRONG_NON_ARITHMETIC_TYPEDEF(type, name, ns)               \
  OUR_STRONG_TYPEDEF_COMMON(type, name, ns)

#define OUR_STRONG_ARITHMETIC_TYPEDEF(type, name, ns)                   \
  OUR_STRONG_TYPEDEF_COMMON(type, name, ns)                             \
                                                                        \
  namespace std                                                         \
  {                                                                     \
      template<>                                                        \
          struct numeric_limits<ns::name>                               \
      {                                                                 \
          static_assert(boost::is_arithmetic<type>::value,              \
                        #type " is not arithmetic");                    \
                                                                        \
          static constexpr bool is_specialized = true;                  \
                                                                        \
          static ns::name                                               \
              min() noexcept                                            \
          {                                                             \
              return ns::name(numeric_limits<type>::min());             \
          }                                                             \
                                                                        \
          static ns::name                                               \
              max() noexcept                                            \
          {                                                             \
              return ns::name(numeric_limits<type>::max());             \
          }                                                             \
                                                                        \
          static ns::name                                               \
              lowest() noexcept                                         \
          {                                                             \
              return ns::name(numeric_limits<type>::lowest());          \
          }                                                             \
                                                                        \
          static constexpr int digits = numeric_limits<type>::digits;   \
          static constexpr int digits10 = numeric_limits<type>::digits10; \
          static constexpr int max_digits10 = numeric_limits<type>::max_digits10; \
          static constexpr bool is_signed = numeric_limits<type>::is_signed; \
          static constexpr bool is_integer = numeric_limits<type>::is_integer; \
          static constexpr bool is_exact = numeric_limits<type>::is_exact; \
          static constexpr int radix = numeric_limits<type>::radix;     \
                                                                        \
          static ns::name                                               \
              epsilon() noexcept                                        \
          {                                                             \
              return ns::name(numeric_limits<type>::epsilon());         \
          }                                                             \
                                                                        \
          static ns::name                                               \
              round_error() noexcept                                    \
          {                                                             \
              return ns::name(numeric_limits<type>::round_error());     \
          }                                                             \
                                                                        \
          static constexpr int min_exponent = numeric_limits<type>::min_exponent; \
          static constexpr int min_exponent10 = numeric_limits<type>::min_exponent10; \
          static constexpr int max_exponent = numeric_limits<type>::max_exponent; \
          static constexpr int max_exponent10 = numeric_limits<type>::max_exponent10; \
                                                                        \
          static constexpr bool has_infinity = numeric_limits<type>::has_infinity; \
          static constexpr bool has_quiet_NaN = numeric_limits<type>::has_quiet_NaN; \
          static constexpr bool has_signaling_NaN = numeric_limits<type>::has_signaling_NaN; \
          static constexpr float_denorm_style has_denorm = numeric_limits<type>::has_denorm; \
          static constexpr bool has_denorm_loss = numeric_limits<type>::has_denorm_loss; \
                                                                        \
          static ns::name                                               \
              infinity() noexcept                                       \
          {                                                             \
              return ns::name(numeric_limits<type>::infinity());        \
          }                                                             \
                                                                        \
          static ns::name                                               \
              quiet_NaN() noexcept                                      \
          {                                                             \
              return ns::name(numeric_limits<type>::quiet_NaN());       \
          }                                                             \
                                                                        \
          static ns::name                                               \
              signaling_NaN() noexcept                                  \
          {                                                             \
              return ns::name(numeric_limits<type>::signaling_NaN());   \
          }                                                             \
                                                                        \
          static ns::name                                               \
              denorm_min() noexcept                                     \
          {                                                             \
              return ns::name(numeric_limits<type>::denorm_min());      \
          }                                                             \
                                                                        \
          static constexpr bool is_iec559 = numeric_limits<type>::is_iec559; \
          static constexpr bool is_bounded = numeric_limits<type>::is_bounded; \
          static constexpr bool is_modulo = numeric_limits<type>::is_modulo; \
                                                                        \
          static constexpr bool traps = numeric_limits<type>::traps;    \
          static constexpr bool tinyness_before = numeric_limits<type>::tinyness_before; \
          static constexpr float_round_style round_style = numeric_limits<type>::round_style; \
      };                                                                \
  }

#endif // OUR_STRONG_TYPEDEF_H_
