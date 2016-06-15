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

#ifdef __clang_analyzer__
#define CLANG_ANALYZER_HINT_NON_NULL(ptr)       \
    ASSERT(ptr != nullptr)

#define CLANG_ANALYZER_HINT_NO_DEAD_STORE(var)  \
    (void)(var)

#else
#define CLANG_ANALYZER_HINT_NON_NULL(ptr)
#define CLANG_ANALYZER_HINT_NO_DEAD_STORE(var)
#endif

#endif // !YT_ASSERT_H_

// Local Variables: **
// mode: c++ **
// End: **
