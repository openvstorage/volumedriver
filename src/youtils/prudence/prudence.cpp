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

#include "../Main.h"
#include "../pstream.h"
#include <poll.h>
#include <prudence.h>
#include <boost/thread.hpp>
#include <boost/filesystem/fstream.hpp>

using namespace youtils;

namespace fs = boost::filesystem;
namespace po = boost::program_options;

namespace
{

fs::path empty;
bool seen_parent_die_signal = false;

void
parent_dies_sighandler(int)
{
    seen_parent_die_signal = true;
}
bool child_died = false;

void
child_dies_handler(int)
{
    child_died = true;
}

using namespace std::string_literals;

const std::map<std::string, int> signal_map =
{
    {"SIGHUP"s, 1},
    {"SIGINT"s, 2},
    {"SIGQUIT"s, 3},
    {"SIGILL"s, 4},
    {"SIGTRAP"s, 5},
    {"SIGABRT"s, 6},
    {"SIGIOT"s, 6},
    {"SIGBUS"s, 7},
    {"SIGFPE"s, 8},
    {"SIGKILL"s, 9},
    {"SIGUSR1"s, 10},
    {"SIGSEGV"s, 11},
    {"SIGUSR2"s, 12},
    {"SIGPIPE"s, 13},
    {"SIGALRM"s,14},
    {"SIGTERM"s, 15}

   //    "SIGSTKFLT	16
    // "SIGCHLD		17
 //    "SIGCONT		18
 // "SIGSTOP		19
 //    "SIGTSTP		20
 // "SIGTTIN		21
 //    "SIGTTOU		22
 // "SIGURG		23
 //    "SIGXCPU		24
 // "SIGXFSZ		25
 //    "SIGVTALRM	26
 // "SIGPROF		27
 //    "SIGWINCH	28
 // "SIGIO		29
    //  "SIGPOLL		SIGIO
};


int
signal_to_num(const std::string signal)
{

    auto it = signal_map.find(signal);
    if(it != signal_map.end())
    {
        return it->second;
    }
    else
    {
        throw "Exception, no such signal";
    }

}
}

struct prudence : public MainHelper
{
    prudence(int argc,
             char** argv)
        : MainHelper(argc, argv)
        , prudence_options_()
        , max_wait_in_seconds_(0)
    {
        prudence_options_.add_options()
            ("start-script",
             po::value<fs::path>(&start_script_)->default_value(empty))
            ("cleanup-script",
             po::value<fs::path>(&cleanup_script_)->default_value(empty))
            ("signal",
             po::value<std::string>(&signal_)->default_value("SIGTERM"))
            ("max-wait-in-seconds",
             po::value<int64_t>(&max_wait_in_seconds_)->default_value(0))
            ("child-death-grace-interval-in-seconds",
             po::value<uint64_t>(&child_death_grace_interval_)->default_value(60))
            ("cleanup-script-wait-in-seconds",
             po::value<uint64_t>(&cleanup_script_wait_in_seconds_)->default_value(60))
            ("child-log-file",
             po::value<fs::path>(&child_log_file)->default_value("prudence-child.log"))
            ;
    }

    void
    log_extra_help(std::ostream& strm) override final
    {
        strm << prudence_options_;
    }

    void parse_command_line_arguments() override final
    {
        parse_unparsed_options(prudence_options_,
                               youtils::AllowUnregisteredOptions::F,
                               vm_);
        the_signal = signal_to_num(signal_);
    }


    void
    setup_logging() override final
    {
        MainHelper::setup_logging();
    }
    DECLARE_LOGGER("PRUDENCE")

    enum class SleepStatus
    {
        TimeOut,
        Interrupted,
        Error
    };


    SleepStatus
    sleep_unblocked(int64_t seconds)
    {
        LOG_INFO("Starting poll");
        sigset_t unblocked_sighup;
        sigemptyset(&unblocked_sighup);
        struct timespec timespec = { seconds , 0};
        // std::vector<struct pollfd> pollfds;
        // pollfds.push_back({-1,0,0});

        auto ret = ppoll(NULL,
                         0,
                         seconds ? &timespec : NULL,
                         &unblocked_sighup);

        switch(ret)
        {
        case 0:
            LOG_INFO("TimeOut of Poll");

            return SleepStatus::TimeOut;
        case -1:
            {
                auto err = errno;
                switch(err)
                {
                case EINTR:
                    LOG_INFO("Poll Interrupted");
                    return SleepStatus::Interrupted;
                default:
                    LOG_INFO("Poll Error: " << strerror(err));
                    return SleepStatus::Error;
                }
            }
        }
        UNREACHABLE;
    }

    bool
    script_file_ok(const fs::path& script_file)
    {
        if(not script_file.empty())
        {
            TODO("y42: Should find the file on $PATH");
            // if(not fs::exists(script_file))
            // {
            //     LOG_FATAL("Could not find the script " << script_file);
            //     return false;

            // }
            // if(not fs::is_regular_file(script_file))
            // {
            //     LOG_FATAL("Script " << script_file << " is not a regular file");
            //     return false;

            // }
            return true;
        }
        return false;

    }

    bool
    run_script(const fs::path& script_file)
    {
        ASSERT(script_file_ok(script_file));
        ASSERT(not child_thread);

        LOG_INFO("Starting program: " << script_file);

        try
        {
            std::vector<std::string> args;
            args.emplace_back(script_file.string());
            child_stream = std::make_unique<redi::ipstream>(args);
        }
        catch(std::exception& e)
        {
            LOG_INFO("Caught exception: " << e.what() << " trying to start the script: " << script_file);
            return false;

        }
        catch(...)
        {
            LOG_INFO("Caught exception trying to start the script: " << script_file);
            return false;
        }

        // Y42: is this strictly necessary?
        while(not child_stream->is_open())
        {
            sleep(1);
        }
        LOG_INFO("Child pid is : " << child_stream->rdbuf()->pid());
        child_thread.reset(new boost::thread([this]()
                                             {
                                                 log_child();
                                             }));


        return true;
    }

    int
    run_cleanup()
    {
        if(child_stream->rdbuf()->exited())
        {

            if(not child_died)
            {
                // Can this deadlock??
                LOG_INFO("childstream exited but not seen the signal");
            }
            else
            {
                LOG_INFO("child already died");
            }

        }
        else
        {
            LOG_INFO("Killing child with the signal " << the_signal);
            child_stream->rdbuf()->kill(the_signal);
            sleep_unblocked(child_death_grace_interval_);
        }


        if(not child_stream->rdbuf()->exited())
        {
            LOG_ERROR("Waited " << child_death_grace_interval_ << " seconds and child didn' die");
            LOG_ERROR("Bashing it's head in now with the blunt instrument called SIGKILL");
            child_stream->rdbuf()->kill(SIGKILL);
            sleep_unblocked(child_death_grace_interval_);
        }
        child_thread->join();
        child_thread.reset(0);

        ASSERT(child_stream->rdbuf()->exited());
        child_stream.reset(0);
        if(script_file_ok(cleanup_script_))
        {
            run_script(cleanup_script_);
            ASSERT(child_stream);
            ASSERT(child_stream->is_open());
            sleep_unblocked(cleanup_script_wait_in_seconds_);
            if(not child_stream->rdbuf()->exited())
            {
                LOG_INFO("Waited " << cleanup_script_wait_in_seconds_ << " seconds and cleanup script didn't die");
                LOG_ERROR("Stabbing it in the back with a SIGKILL blade");
                child_stream->rdbuf()->kill(SIGKILL);
                sleep_unblocked(child_death_grace_interval_);
            }
            child_thread->join();

        }
        else
        {
            LOG_INFO("No Cleanup Script to run");
        }

        return Success;
    }

    void
    log_child()
    {
        std::string line;
        while(std::getline(*child_stream, line))
        {
            LOG_INFO(line);
        }
    }


    int
    run() override final
    {
        if(not script_file_ok(start_script_))
        {
            LOG_FATAL("Could not find a start script");
            return StartScriptNotOk;

        }

        auto ret = 0;

        // set it so we get a term in our parent dies
        LOG_INFO("Setting parent to signal us when it dies");

        ret = prctl(PR_SET_PDEATHSIG, parent_dies_signal);
        if(ret != 0)
        {
            auto err = errno;
            LOG_FATAL("could not set parent notification when it dies: " << strerror(err));
            return CouldNotSetNotification;
        }
        // set the signal handler
        LOG_INFO("Setting signal handler for when parent dies");

        auto sigret = signal(parent_dies_signal, parent_dies_sighandler);
        if(sigret == SIG_ERR)
        {
            auto err = errno;
            LOG_FATAL("Could not set singnal handler for the parent_dies_signal: " << strerror(err));
            return CouldNotSetSignalHandler;

        }
        // set signal handler
        LOG_INFO("Setting signal handler when child dies");

        sigret = signal(SIGCHLD, child_dies_handler);
        if(sigret == SIG_ERR)
        {
            auto err = errno;
            LOG_FATAL("Could not set signal handler for the sigchild signal: " << strerror(err));
            return CouldNotSetSignalHandler;
        }

        LOG_INFO("Blocking all signals")
        sigset_t blocked_sighup;
        ret = sigemptyset(&blocked_sighup);
        if(ret != 0)
        {
            auto err = errno;
            LOG_FATAL("Could not set the empty signal: " << strerror(err));
            return CouldNotBlockSignals;
        }

        ret = sigaddset(&blocked_sighup, parent_dies_signal);
        if(ret != 0)
        {
            auto err = errno;
            LOG_FATAL("Could not add the parent_dies_signal: " << strerror(err));
            return CouldNotBlockSignals;
        }

        ret = sigaddset(&blocked_sighup, SIGCHLD);
        if(ret != 0)
        {
            auto err = errno;
            LOG_FATAL("Could not add the sigchild signal: " << strerror(err));
            return CouldNotBlockSignals;
        }

        ret = sigprocmask(SIG_BLOCK, &blocked_sighup, NULL);
        if(ret != 0)
        {
            auto err = errno;
            LOG_FATAL("Could not block the signals: " << strerror(err));
            return CouldNotBlockSignals;
        }
        LOG_INFO("Checking whether parent has died and we haven't noticed");

        if (getppid() == 1)
        {
            LOG_INFO("Parent died already not starting anything");
            return Success;
        }
        LOG_INFO("Running the start script");
        if(not run_script(start_script_))
        {
            LOG_FATAL("Problem starting the script");
            return ProblemRunningTheScript;
        }

        // Might sleep forever if:
        // * child doesn't die AND
        // * parent doesn't die AND
        // * no timeout was set AND
        switch(sleep_unblocked(max_wait_in_seconds_))
        {

        case SleepStatus::TimeOut:
            LOG_INFO("Zero return from ppol, timeout...");
            return run_cleanup();


        case SleepStatus::Interrupted:
            {
                if(seen_parent_die_signal or child_died)
                {
                    return run_cleanup();
                }

                else
                {
                    LOG_FATAL("Seen a signal but not sigurs1 or child died");
                    LOG_FATAL("Just exiting, manual cleanup required");
                    return 1;
                }
            }

        case SleepStatus::Error:
            return PollError;

        };
        UNREACHABLE;
    };

    po::options_description prudence_options_;
    fs::path start_script_;
    fs::path cleanup_script_;
    fs::path child_log_file;

    std::string signal_;
    int the_signal;
    int64_t max_wait_in_seconds_;
    uint64_t child_death_grace_interval_;
    uint64_t cleanup_script_wait_in_seconds_;

    std::unique_ptr<redi::ipstream> child_stream;

    std::unique_ptr<boost::thread> child_thread;

};


MAIN(prudence);
