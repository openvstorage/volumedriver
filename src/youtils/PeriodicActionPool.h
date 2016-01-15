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

#ifndef YT_PERIODIC_ACTION_POOL_H_
#define YT_PERIODIC_ACTION_POOL_H_

#include "Logging.h"
#include "PeriodicAction.h"

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread.hpp>

namespace youtils
{

class PeriodicActionPool
    : public boost::enable_shared_from_this<PeriodicActionPool>
{
public:
    using Ptr = boost::shared_ptr<PeriodicActionPool>;

    static Ptr
    create(const std::string& name,
           const size_t nthreads)
    {
        return Ptr(new PeriodicActionPool(name,
                                          nthreads));
    }

    ~PeriodicActionPool();

    PeriodicActionPool(const PeriodicActionPool&) = delete;

    PeriodicActionPool&
    operator=(const PeriodicActionPool&) = delete;

    class Task
    {
    public:
        ~Task();

        Task(const Task&) = delete;

        Task&
        operator=(const Task&) = delete;

        Task(Task&&) = delete;

        Task&
        operator=(Task&&) = delete;

    private:
        DECLARE_LOGGER("PeriodicActionPoolTask");

        friend class PeriodicActionPool;

        Ptr pool_;
        std::string name_;
        const std::atomic<uint64_t>& period_;
        PeriodicAction::AbortableAction action_;
        bool period_in_seconds_;
        boost::asio::steady_timer deadline_;
        boost::mutex lock_;
        boost::condition_variable cond_;
        bool stop_;
        bool stopped_;

        Task(const std::string& name,
             PeriodicAction::AbortableAction,
             const std::atomic<uint64_t>& period,
             const bool period_in_seconds,
             const std::chrono::milliseconds& ramp_up,
             Ptr pool);

        void
        run_(const std::chrono::milliseconds&);
    };

    std::unique_ptr<Task>
    create_task(const std::string& name,
                PeriodicAction::AbortableAction action,
                const std::atomic<uint64_t>& period,
                const bool period_in_seconds = true,
                const std::chrono::milliseconds& ramp_up = std::chrono::milliseconds(0));

private:
    DECLARE_LOGGER("PeriodicActionPool");

    boost::asio::io_service io_service_;
    decltype(io_service_)::work work_;
    boost::thread_group threads_;
    const std::string name_;

    PeriodicActionPool(const std::string& name,
                       size_t nthreads);

    void
    run_();

    void
    stop_();
};

}

#endif // !YT_PERIODIC_ACTION_POOL_H_
