// Copyright 2015 iNuron NV
//
// licensed under the Apache license, Version 2.0 (the "license");
// you may not use this file except in compliance with the license.
// You may obtain a copy of the license at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the license is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the license for the specific language governing permissions and
// limitations under the license.

#ifndef VFS_LOCKED_PYTHON_CLIENT_H_
#define VFS_LOCKED_PYTHON_CLIENT_H_

#include "PythonClient.h"

#include <boost/enable_shared_from_this.hpp>
#include <boost/python/list.hpp>
#include <boost/python/object.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>

#include <youtils/HeartBeatLockService.h>
#include <youtils/Logging.h>

#include <pstreams/pstream.h>

namespace volumedriverfs
{

// A Python context manager that uses the global locking mechanism:
// * a thread is spawned on enter() waiting for a promise to be fulfilled
// * exit() fulfills the promise
// * calls requiring fencing are performed in child processes (using exec!)
//   as we want to keep the Python process running this code alive.
class LockedPythonClient final
    : public boost::enable_shared_from_this<LockedPythonClient>
    , public PythonClient
{
public:
    using Ptr = boost::shared_ptr<LockedPythonClient>;

    static Ptr
    create(const std::string& cluster_id,
           const std::vector<ClusterContact>&,
           const std::string& volume_id,
           const unsigned update_interval_secs = 3,
           const unsigned grace_period_secs = 5);

    ~LockedPythonClient();

    LockedPythonClient(const LockedPythonClient&) = delete;

    LockedPythonClient&
    operator=(const LockedPythonClient&) = delete;

    Ptr
    enter();

    void
    exit(boost::python::object& /* exc_type */,
         boost::python::object& /* exc_value */,
         boost::python::object& /* traceback */);

    boost::python::list
    get_scrubbing_work();

    void
    apply_scrubbing_result(const std::string& res);

    std::string
    scrub(const std::string& scrub_work,
          const std::string& scratch_dir,
          const uint64_t region_size_exponent,
          const float fill_ratio,
          const bool verbose,
          const std::string& scrubber_name,
          const youtils::Severity,
          const boost::optional<std::string>& logfile);

    // Make the following ones private, with access granted only to the
    // global locking code?
    std::string
    info();

    void
    operator()();

    const youtils::GracePeriod&
    grace_period() const
    {
        return grace_period_;
    }

private:
    DECLARE_LOGGER("LockedPythonClient");

    LockedPythonClient(const std::string& cluster_id,
                       const std::vector<ClusterContact>&,
                       const std::string& volume_id,
                       const youtils::UpdateInterval&,
                       const youtils::GracePeriod&);

    const std::string volume_id_;
    const youtils::GracePeriod grace_period_;
    const youtils::UpdateInterval update_interval_;

    using Promise = boost::promise<void>;
    boost::optional<Promise> promise_;
    Promise start_promise_;

    using Future = boost::shared_future<void>;
    Future future_;
    Future start_future_;

    boost::thread thread_;

    boost::mutex child_lock_;

    using ChildOutputStreamPtr = boost::shared_ptr<redi::ipstream>;
    ChildOutputStreamPtr child_;

    using LockService = youtils::HeartBeatLockService;
    using LockedSection =
        LockService::WithGlobalLock<youtils::ExceptionPolicy::ThrowExceptions,
                                    LockedPythonClient>;

    std::unique_ptr<LockedSection> locked_section_;
};

}

#endif // !VFS_LOCKED_PYTHON_CLIENT_H_
