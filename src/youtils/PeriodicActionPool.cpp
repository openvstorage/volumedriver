#include "Assert.h"
#include "Catchers.h"
#include "PeriodicActionPool.h"

namespace youtils
{

namespace ba = boost::asio;
namespace bs = boost::system;
namespace sc = std::chrono;

PeriodicActionPool::PeriodicActionPool(const std::string& name,
                                       size_t nthreads)
    : work_(io_service_)
    , name_(name)
{
    THROW_WHEN(nthreads == 0);

    try
    {
        for (size_t i = 0; i < nthreads; ++i)
        {
            threads_.create_thread(boost::bind(&PeriodicActionPool::run_,
                                               this));
        }

        LOG_INFO(name_ << ": alive and well");
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR(name_ << ": failed to create thread pool: " << EWHAT);
            stop_();
            throw;
        });
}

PeriodicActionPool::~PeriodicActionPool()
{
    try
    {
        stop_();
    }
    CATCH_STD_ALL_LOG_IGNORE(name_ << ": failed to stop");
}

void
PeriodicActionPool::stop_()
{
    LOG_INFO(name_ << ": stopping");
    io_service_.stop();
    threads_.join_all();
}

void
PeriodicActionPool::run_()
{
    while (true)
    {
        try
        {
            io_service_.run();
            LOG_INFO(name_ << ": I/O service exited");
            return;
        }
        CATCH_STD_ALL_LOG_IGNORE(name_ << ": caught exception in worker thread");
    }
}

PeriodicActionPool::Task::Task(const std::string& name,
                               PeriodicAction::AbortableAction action,
                               const std::atomic<uint64_t>& period,
                               const bool period_in_seconds,
                               const sc::milliseconds& ramp_up,
                               PeriodicActionPool::Ptr pool)
    : pool_(pool)
    , name_(name)
    , period_(period)
    , action_(std::move(action))
    , period_in_seconds_(period_in_seconds)
    , deadline_(pool_->io_service_)
    , stop_(false)
    , stopped_(false)
{
    run_(ramp_up);
}

PeriodicActionPool::Task::~Task()
{
    try
    {
        boost::unique_lock<decltype(lock_)> u(lock_);

        stop_ = true;

        deadline_.expires_from_now(sc::milliseconds(0));

        cond_.wait(u,
                   [&]() -> bool
                   {
                       return stopped_ == true;
                   });
    }
    CATCH_STD_ALL_LOG_IGNORE(name_ << ": error while cancelling timer");
}

void
PeriodicActionPool::Task::run_(const sc::milliseconds& timeout)
{
    auto fun([this](const bs::error_code& ec)
             {
                 boost::lock_guard<decltype(lock_)> g(lock_);

                 PeriodicActionContinue cont = PeriodicActionContinue::T;

                 if (ec == bs::errc::operation_canceled or
                     ec == ba::error::operation_aborted)
                 {
                     LOG_INFO(name_ << ": timer cancelled");
                     cont = PeriodicActionContinue::F;
                 }
                 else
                 {
                     if (ec)
                     {
                         LOG_ERROR(name_ << ": timer reported error " <<
                                   ec.message());
                     }
                     else
                     {
                         LOG_TRACE(name_ << ": timer fired");
                     }

                     if (not stop_)
                     {
                         try
                         {
                             cont = action_();
                         }
                         CATCH_STD_ALL_LOG_IGNORE(name_ << ": caught exception");
                     }
                     else
                     {
                         cont = PeriodicActionContinue::F;
                     }
                 }

                 if (cont == PeriodicActionContinue::T)
                 {
                     run_(period_in_seconds_ ?
                          sc::milliseconds(period_ * 1000) :
                          sc::milliseconds(period_));
                 }
                 else
                 {
                     LOG_INFO(name_ << ": bailing out");
                     stopped_ = true;
                     cond_.notify_one();
                 }
             });

    deadline_.expires_from_now(timeout);
    deadline_.async_wait(std::move(fun));
}

std::unique_ptr<PeriodicActionPool::Task>
PeriodicActionPool::create_task(const std::string& name,
                                PeriodicAction::AbortableAction action,
                                const std::atomic<uint64_t>& period,
                                const bool period_in_seconds,
                                const std::chrono::milliseconds& ramp_up)
{
    return std::unique_ptr<Task>(new Task(name,
                                          std::move(action),
                                          period,
                                          period_in_seconds,
                                          ramp_up,
                                          shared_from_this()));
}

}
