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

#ifndef WITH_GLOBAL_LOCK_H
#define WITH_GLOBAL_LOCK_H

#include "Assert.h"
#include "GlobalLockService.h"
#include "Logging.h"
#include "OurStrongTypedef.h"

#include <string>

#include <boost/scope_exit.hpp>
#include <boost/thread.hpp>

OUR_STRONG_ARITHMETIC_TYPEDEF(uint64_t, NumberOfRetries, youtils);

namespace youtils
{
namespace
{

inline DECLARE_LOGGER("trampoline_template");

template<typename A,
         void(A::*mem_fun)(const std::string&)>
static void
trampoline_template(void* data,
                    const std::string& reason)
{
    A* a_pointer = reinterpret_cast<A*>(data);
    VERIFY(data);
    (a_pointer->*mem_fun)(reason);
}
}

struct WithGlobalLockExceptions
{
    MAKE_EXCEPTION(WithGlobalLockException, fungi::IOException);
    MAKE_EXCEPTION(UnableToGetLockException, WithGlobalLockException);
    MAKE_EXCEPTION(LostLockException, WithGlobalLockException);
    MAKE_EXCEPTION(CouldNotInterruptCallable, WithGlobalLockException);

   class CallableException
       : WithGlobalLockException
    {
    public:
        CallableException(std::exception_ptr except)
            : WithGlobalLockException("Callable threw exception")
            , exception_(except)
        {}

        std::exception_ptr exception_;
    };
};

enum class ExceptionPolicy
{
    ThrowExceptions,
    DisableExceptions
};

template<typename T>
struct ConstructorArguments
{};

DECLARE_DURATION_TYPE(ConnectionRetryTimeout);

template<ExceptionPolicy exception_policy,
         typename Callable,
         std::string(Callable::*info_member_function)(),
         typename GlobalLockService,
         typename... Args>
class WithGlobalLock
    : public WithGlobalLockExceptions, public GlobalLockedCallable
{
public:
    WithGlobalLock(boost::reference_wrapper<Callable> callable,
                   const NumberOfRetries retry_connection_times,
                   const ConnectionRetryTimeout& connection_retry_timeout,
                   Args... args)
        : callable_(callable)
        , has_lock_(false)
        , clientFinished_(false)
        , retry_connection_times_(retry_connection_times)
        , connection_retry_timeout_(connection_retry_timeout)
        , global_lock_service_(boost::unwrap_ref(callable_).grace_period(),
                               &trampoline_template<WithGlobalLock,
                                                    &WithGlobalLock::lost_lock_callback>,
                               this,
                               std::forward<Args>(args)...)
    {
        // static_assert(dynamic_cast<GlobalLockService*>(&global_lock_service_) != 0, "Not a GlobalLockService implementor");
    }

    static inline const NumberOfRetries
    retry_connection_times_default()
    {
        static const NumberOfRetries val = NumberOfRetries(std::numeric_limits<uint64_t>::max());
        return val;
    }

    static const ConnectionRetryTimeout&
    connection_retry_timeout_default()
    {
        static const ConnectionRetryTimeout val(boost::posix_time::milliseconds(100));
        return val;
    }

    virtual const GracePeriod&
    grace_period() const
    {
        return boost::unwrap_ref(callable_).grace_period();
    }

    void
    operator()()
    {
        LOG_INFO("Trying to get the global lock");
        try
        {
            uint64_t retries = 0;
            boost::unique_lock<boost::mutex> lock(has_lock_mutex_);
            do
            {
                has_lock_ = global_lock_service_.lock();
                if(not has_lock_ and
                   retries < retry_connection_times_)
               {
                    boost::this_thread::sleep(connection_retry_timeout_);
                    ++retries;

                }
            }
            while(not has_lock_ and
                  retries < retry_connection_times_);

            if(has_lock_)
            {
                BOOST_SCOPE_EXIT_TPL((&lock)
                                     (&global_lock_service_)
                                     (&has_lock_))
                {
                    // no need to lock again... this is in the scope of the lock above!!
                    LOG_INFO("Releasing the global lock");
                    try
                    {
                        has_lock_ = false;
                        lock.unlock();
                        lock.release();
                        global_lock_service_.unlock();
                    }
                    catch(std::exception& e)
                    {
                        LOG_INFO("problem unlocking the lock " << e.what());
                    }

                    catch(...)
                    {
                        LOG_INFO("unknown problem unlocking the lock");
                    }

                } BOOST_SCOPE_EXIT_END;

                LOG_INFO("Got the global lock, starting Callable");

                ASSERT(has_lock_);

                std::exception_ptr no_exception;

                boost::thread client_thread(boost::bind(&WithGlobalLock::clientFun_, this));

                while(true)
                {
                    has_lock_condition_variable_.wait(lock);
                    if(clientFinished_)
                    {
                        LOG_INFO("Notified while still owning the lock, joining callable thread");
                        client_thread.join();
                        if(thrown_exception_ == no_exception)
                        {
                            LOG_INFO("Exiting normally");
                            return;
                        }
                        else
                        {
                            LOG_INFO("Rethrowing exception thrown by the callable thread");
                            throw CallableException(thrown_exception_);
                        }
                    }
                    if(not has_lock_)
                    {
                        LOG_INFO("Notified while not owning the lock, interrupting callable thread");
                        client_thread.interrupt();
                        LOG_INFO("Waiting " <<
                                 static_cast<boost::posix_time::time_duration>(grace_period()).total_milliseconds()
                                << " milliseconds for joining callable");

                        if(has_lock_condition_variable_.timed_wait(lock,
                                                                   grace_period()))
                        {
                            client_thread.join();
                            if(thrown_exception_ == no_exception)
                            {
                                LOG_INFO("Interrupted thread has exited normally");
                                return;
                            }
                            else
                            {
                                try
                                {
                                    std::rethrow_exception(thrown_exception_);
                                }
                                catch(boost::thread_interrupted& e)
                                {
                                    throw LostLockException("thread lost the lock and was interrupted");

                                }
                                catch(...)
                                {
                                    LOG_INFO("Interrupted thread threw exception");
                                    throw CallableException(thrown_exception_);
                                }
                            }
                        }
                        else
                        {
                            LOG_FATAL("Client thread was not interrupted within the timeout, returning");
                            switch (exception_policy)
                            {
                            case ExceptionPolicy::ThrowExceptions:
                                LOG_FATAL("Throwing Exception which should kill the executable");
                                throw CouldNotInterruptCallable("Could not interrupt callable");
                            case ExceptionPolicy::DisableExceptions:
                                LOG_FATAL("Aborting execution");
                                std::terminate();
                            }
                        }
                    }

                    ASSERT(has_lock_ and not clientFinished_);
                    LOG_WARN("Spurious wake-up, these should not occur too often");
                }
            }
            else
            {
                // Y42 Retry? not throw?
                LOG_INFO("Could not grab the lock");
                throw UnableToGetLockException("could not grab the lock");
            }
        }
        catch(std::exception& e)
        {
            switch(exception_policy)
            {
            case ExceptionPolicy::ThrowExceptions:
                LOG_WARN("Exception trying to exit the main method of the WithGlobalLock: " << e.what() << ", reraising it");
                throw;
            case ExceptionPolicy::DisableExceptions:
                LOG_WARN("Exception trying to exit the main method of the WithGlobalLock: " << e.what() << ", ignoring it");
                exit_exception = std::current_exception();
            }
        }
    }

    void
    lost_lock_callback(const std::string& reason)
    {
        LOG_INFO("Disconnected: " << reason);

        {
            boost::unique_lock<boost::mutex> lock(has_lock_mutex_);
            if(has_lock_)
            {
                has_lock_ = false;
                has_lock_condition_variable_.notify_all();
            }
        }
    }

private:
    DECLARE_LOGGER("WithGlobalLock");

    boost::reference_wrapper<Callable> callable_;

    bool has_lock_;
    bool clientFinished_;
    boost::mutex has_lock_mutex_;
    boost::condition_variable has_lock_condition_variable_;
    const NumberOfRetries retry_connection_times_;
    const boost::posix_time::time_duration connection_retry_timeout_;
    std::exception_ptr thrown_exception_;
    GlobalLockService global_lock_service_;

    void
    clientFun_()
    {
        LOG_INFO("Starting Callable ");
        try
        {
            callable_.get()();
        }
        catch(...)
        {
            thrown_exception_ = std::current_exception();
        }

        LOG_INFO("Finished Callable ");

        {
            boost::unique_lock<boost::mutex> lock(has_lock_mutex_);
            clientFinished_ = true;
            has_lock_condition_variable_.notify_all();
        }
    }

public:
    std::exception_ptr exit_exception;
};

}

#endif // WITH_GLOBAL_LOCK_H

// Local Variables: **
// mode: c++ **
// End: **
