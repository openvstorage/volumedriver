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

#ifndef DEFERRED_FILE_REMOVER_H_
#define DEFERRED_FILE_REMOVER_H_

#include "Logging.h"

#include <boost/filesystem.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

namespace youtilstest
{

class DeferredFileRemoverTest;

}

namespace youtils
{

class DeferredFileRemover
{
public:
    explicit DeferredFileRemover(const boost::filesystem::path&);

    ~DeferredFileRemover();

    DeferredFileRemover(const DeferredFileRemover&) = delete;

    DeferredFileRemover&
    operator=(const DeferredFileRemover&) = delete;

    void
    schedule_removal(const boost::filesystem::path&);

    boost::filesystem::path
    path() const
    {
        return path_;
    }

private:
    DECLARE_LOGGER("DeferredFileRemover");

    enum class State
    {
        Idle,
        Work,
        Stop
    };

    boost::filesystem::path path_;
    boost::mutex lock_;
    boost::condition_variable cond_;
    boost::condition_variable caller_cond_;
    State state_;
    boost::thread thread_;

    friend class youtilstest::DeferredFileRemoverTest;

    // not bulletproof against newly added work; only for testing purposes.
    void
    wait_for_completion_();

    void
    work_();

    void
    collect_garbage_();
};

}

#endif // !DEFERRED_FILE_REMOVER_H_
