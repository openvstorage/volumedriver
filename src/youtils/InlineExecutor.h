// Copyright (C) 2017 iNuron NV
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

#ifndef YT_INLINE_EXECUTOR_H_
#define YT_INLINE_EXECUTOR_H_

// has to be included before ..inline_executor.hpp as the latter depends on
// a declaration in the former.
#include <boost/thread/executors/thread_executor.hpp>
#include <boost/thread/executors/inline_executor.hpp>

namespace youtils
{

struct InlineExecutor
{
    // TODO:
    // Sharing boost::inline_executor across threads might not be a good idea as it
    // uses a lock internally. Look into maintaining per-thread instances or provide
    // a custom inline executor.
    static boost::executors::inline_executor&
    get()
    {
        static boost::executors::inline_executor e;
        return e;
    }
};

}

#endif // !YT_INLINE_EXECUTOR_H_
