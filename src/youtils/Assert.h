// Copyright 2015 iNuron NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef YT_ASSERT_H_
#define YT_ASSERT_H_

#include "Logging.h"
#include "IOException.h"

#include <assert.h>

#define VERIFY(x)                                                       \
    do                                                                  \
    {                                                                   \
        if(not (x))                                                     \
        {                                                               \
            LOG_FATAL(__PRETTY_FUNCTION__ << ": " #x);                  \
            assert(x);                                                  \
            throw fungi::IOException("ASSERT: " #x, __PRETTY_FUNCTION__); \
        }                                                               \
    } while(false)                                                      \


#define ASSERT(x) assert(x)

#ifndef NDEBUG
#define DEBUG_CHECK(x)                                                  \
    if (not (x))                                                        \
    {                                                                   \
        LOG_FATAL( __PRETTY_FUNCTION__ << ": " #x);                     \
        assert(x);                                                      \
    }
#else
#define DEBUG_CHECK(x)
#endif

#ifndef NDEBUG
#define ASSERT_BLOCK {
#else
#define ASSERT_BLOCK if(false){
#endif

#ifndef NDEBUG
#define END_ASSERT_BLOCK }
#else
#define END_ASSERT_BLOCK }
#endif

#ifndef NDEBUG

#define ASSERT_LOCKABLE_LOCKED(l)               \
    ASSERT(not (l).try_lock())

#define ASSERT_LOCKABLE_NOT_LOCKED(l)           \
    do                                          \
    {                                           \
        ASSERT((l).try_lock());                 \
        (l).unlock();                           \
    }                                           \
    while (false)

#else

#define ASSERT_LOCKABLE_LOCKED(l)               \
    do {} while (false)

#define ASSERT_LOCKABLE_NOT_LOCKED(l)           \
    do {} while (false)

#endif

#define THROW_WHEN(x, ...)                                              \
    if(x)                                                               \
        throw fungi::IOException("THROW_WHEN"  #x, __PRETTY_FUNCTION__); \


#define THROW_UNLESS(x,...)                                             \
    if(not (x))                                                           \
        throw fungi::IOException("THROW_UNLESS: " #x, __PRETTY_FUNCTION__); \

// #if __GNUG__ && __GNUC_MAJOR__ <= 4 && __GNUC_MINOR__ < 5
// #define UNREACHEABLE VERIFY(0);
// #else
#define UNREACHABLE   __builtin_unreachable();
// #endif

// TODO: support for other compilers
#define PRAGMA__(p)                             \
    _Pragma(#p)

#ifdef SUPPRESS_WARNINGS

#define PRAGMA_IGNORE_WARNING_BEGIN(d)          \
    PRAGMA__(GCC diagnostic push)               \
    PRAGMA__(GCC diagnostic ignored d)

#define PRAGMA_IGNORE_WARNING_END               \
    PRAGMA__(GCC diagnostic pop)                \

#define PRAGMA_WARNING(w)

#else

#define PRAGMA_IGNORE_WARNING_BEGIN(d)

#define PRAGMA_IGNORE_WARNING_END

#define PRAGMA_WARNING(w)                       \
    PRAGMA__(GCC warning w)

#endif

#define TODO(t)                                 \
    PRAGMA_WARNING(t)

#endif // !YT_ASSERT_H_

// Local Variables: **
// mode: c++ **
// End: **
