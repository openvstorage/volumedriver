// Copyright 2015 Open vStorage NV
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

#include <poll.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/types.h>
#include <unistd.h>

#include <boost/program_options.hpp>
#include <boost/scope_exit.hpp>
#include <boost/thread.hpp>

#include <youtils/BuildInfo.h>
#include <youtils/Logger.h>
#include <youtils/Logging.h>
#include <youtils/WithGlobalLock.h>

#include <backend/BackendConfig.h>
#include <backend/BackendConnectionManager.h>
#include <backend/GlobalLockService.h>
#include <youtils/Main.h>

namespace po = boost::program_options;
namespace fs = boost::filesystem;
namespace yt = youtils;

namespace
{

boost::condition_variable cond_var;
boost::mutex cond_var_mutex;
bool stop = false;

DECLARE_LOGGER("locked_executable");

// #warning "Refactor and put in the MainHelper Class"
void
shutdown()
{
    boost::lock_guard<boost::mutex> g(cond_var_mutex);
    if (not stop)
    {
        LOG_INFO("Signalling ourselves");
        // grabbing the lock failed - notify the sighandler thread.
        kill(getpid(), SIGUSR1);
    }
}
}

struct SignalHandler
{
    SignalHandler()
        : sigfd_(-1)
    {
        sigset_t mask;
        int ret = ::sigemptyset(&mask);
        VERIFY(ret == 0);

        ret = ::sigaddset(&mask, SIGUSR1);
        VERIFY(ret == 0);

        ret = ::pthread_sigmask(SIG_BLOCK, &mask, 0);
        VERIFY(ret == 0);

        sigfd_ = ::signalfd(-1, &mask, 0);
        VERIFY(sigfd_ >= 0);
    }

    void
    operator()(uint64_t timeout) const
    {
        VERIFY(timeout < static_cast<uint64_t>(std::numeric_limits<int>::max()));

        pollfd pfd = { sigfd_, POLLIN, 0 };
        int ret = ::poll(&pfd, 1, timeout > 0 ? timeout : -1);
        // handle EAGAIN etc?
        VERIFY(ret >= 0);

        if (ret == 0)
        {
            LOG_INFO("timeout");
        }
        else
        {
            signalfd_siginfo si;
            ssize_t res = ::read(sigfd_, &si, sizeof(si));
            VERIFY(res == sizeof(si));
            VERIFY(si.ssi_signo == SIGUSR1);

            LOG_INFO("PID " << si.ssi_pid << " kindly asked us to exit");
        }

        boost::unique_lock<boost::mutex> u(cond_var_mutex);
        stop = true;
        cond_var.notify_all();
    }

private:
    DECLARE_LOGGER("SignalHandler");
    int sigfd_;
};

struct MyCallable
    : public youtils::GlobalLockedCallable
{
    MyCallable(const youtils::GracePeriod& grace_period,
               const uint64_t timeout = 0)
        : timeout_(timeout)
        , grace_period_(grace_period)
    {}

    std::string
    info()
    {
        std::stringstream ss;
        ss << "MyCallable";
        return ss.str();
    }

    void
    operator()()
    {
        std::cout << "OK tock laken" << std::endl;

        boost::unique_lock<boost::mutex> u(cond_var_mutex);
        while(not stop)
        {
            cond_var.wait(u);
        }

        std::cout << "OK tolc kanek" << std::endl;
    }

    virtual const youtils::GracePeriod&
    grace_period() const
    {
        return grace_period_;
    }

    const uint64_t timeout_;
    const youtils::GracePeriod grace_period_;

    DECLARE_LOGGER("MyCallable");
};

template <typename T1>
struct GlobalCallableWrapper
    : public youtils::GlobalLockedCallable
{
    typedef typename backend::GlobalLockService::WithGlobalLock<youtils::ExceptionPolicy::ThrowExceptions,
                                                                          T1>::type_ CallableT;

    GlobalCallableWrapper(CallableT& callable)
        : callable_(callable)
    {}

    void
    operator()()
    {
        try
        {
            callable_();
        }
        catch(youtils::WithGlobalLockExceptions::UnableToGetLockException& e)
        {
            shutdown();
            std::cout << "FAILURE ta toke lack" << std::endl;
        }
        catch(...)
        {
            shutdown();
        }
    }

    virtual const youtils::GracePeriod&
    grace_period() const
    {
        return callable_.grace_period();
    }

    CallableT& callable_;
};


class LockedExecutableMain : public youtils::MainHelper
{
public:
    LockedExecutableMain(int argc,
                         char** argv)
        : MainHelper(argc,
                     argv)
        , normal_options_("Normal options")
    {
        normal_options_.add_options()
            ("namespace",
             po::value<std::string>(&ns_temp)->required(),
             "namespace to grab a lock in")
            ("time-milliseconds",
             po::value<uint64_t>(&timeout_)->default_value(0),
             "time in milliseconds the code should run")
            ("backend-config-file,c",
             po::value<std::string>(&backend_config_file_)->required(),
             "backend config file")
            ("num-retries",
             po::value<uint64_t>(&num_retries_)->default_value(1),
             "number of retries to get the lock")
            ("lock-session-timeout",
             po::value<uint64_t>(&lock_session_timeout_seconds_)->default_value(30),
             "Lock session timeout in seconds -- time between heartbeats")
            ("grace_period_in_seconds",
             po::value<uint64_t>(&grace_period_)->default_value(10),
             "grace period in seconds");

    }

    virtual void
    log_extra_help(std::ostream& strm)
    {
        strm << normal_options_;
    }


    virtual void
    parse_command_line_arguments()
    {
        parse_unparsed_options(normal_options_,
                               yt::AllowUnregisteredOptions::T,
                               vm_);

        ns_.reset(new backend::Namespace(ns_temp));
    }

    virtual void
    setup_logging()
    {
        MainHelper::setup_logging();
    }

    virtual int
    run()
    {
        pt_.put("version", 1);

        auto bcm(backend::BackendConnectionManager::create(pt_));

        typedef GlobalCallableWrapper<MyCallable>::CallableT CallableT;

        MyCallable my_callable(youtils::GracePeriod(boost::posix_time::seconds(grace_period_)),
                               timeout_);

        CallableT callable(boost::ref(my_callable),
                           youtils::NumberOfRetries(num_retries_),
                           CallableT::connection_retry_timeout_default(),
                           bcm,
                           backend::UpdateInterval(boost::posix_time::seconds(lock_session_timeout_seconds_)),
                           *ns_);

        GlobalCallableWrapper<MyCallable> callable2(callable);

        boost::thread t(boost::ref(callable2));
        sighandler_(timeout_);
        t.join();
        return 0;
    }

    SignalHandler sighandler_;
    po::options_description normal_options_;

    std::unique_ptr<backend::Namespace> ns_;
    std::string ns_temp;

    uint64_t timeout_;
    std::string backend_config_file_;
    std::string backend_type_;
    uint64_t lock_session_timeout_seconds_;
    uint64_t num_retries_;
    uint64_t grace_period_;
    boost::property_tree::ptree pt_;
};

MAIN(LockedExecutableMain)

// Local Variables: **
// mode: c++ **
// End: **
