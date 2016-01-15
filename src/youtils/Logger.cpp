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

#include "Assert.h"
#include "FileUtils.h"
#include "Logger.h"
#include "LoggerPrivate.h"

#include <stdlib.h>
#include <sys/time.h>

#include <atomic>
#include <exception>
#include <functional>
#include <typeinfo>
#include <unordered_map>

#include <boost/atomic.hpp>

#include <boost/core/null_deleter.hpp>

#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/path.hpp>

#include <boost/log/attributes.hpp>
#include <boost/log/attributes/attribute_set.hpp>
#include <boost/log/attributes/attribute_value_set.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/core.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/utility/exception_handler.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/string_literal.hpp>

#include <boost/thread.hpp>

namespace youtils
{

namespace bl = boost::log;
namespace fs = boost::filesystem;

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", Severity)
BOOST_LOG_ATTRIBUTE_KEYWORD(id, LOGGER_ATTRIBUTE_ID, LOGGER_ATTRIBUTE_ID_TYPE)
BOOST_LOG_ATTRIBUTE_KEYWORD(subsystem, LOGGER_ATTRIBUTE_SUBSYSTEM, LOGGER_ATTRIBUTE_SUBSYSTEM_TYPE)

namespace
{

// NOTE: This is currently based on the assumption that we typically don't use filters.
// IOW filters are intended for debugging purposes at the moment. If that should change
// some improvements might be in order (after profiling, of course). E.g. each logger
// could have a unique uint32_t or uint64_t as key which could be used as key instead of a
// string.
// Profiling also shows that the filter table is (unsurprisingly) pretty popular and that
// it really helps if it is a nullptr (opposed to an empty unordered map).
using FilterTable = std::unordered_map<Logger::filter_t::first_type,
                                       Logger::filter_t::second_type>;

// LOCKING:
// * read access to the filter table and the global severity is lock free
//   (but uses atomics).
// * updates to the filter table require a mutex to prevent concurrent updates
// * update_lock also protects logging setup/teardown

using FilterTablePtr = boost::shared_ptr<FilterTable>;
FilterTablePtr filter_table;
boost::atomic<Severity> global_severity;

boost::mutex update_lock;

const std::string thread_id_key("ThreadID");
const std::string timestamp_key("TimeStamp");

// slow path, we prefer convenience (call sites not having to check for nullptrs)
// over efficiency.
template<typename F>
void
update_filter_table(F&& fun)
{
    boost::lock_guard<decltype(update_lock)> g(update_lock);

    FilterTablePtr old(boost::atomic_load(&filter_table));
    FilterTablePtr table(old ?
                         boost::make_shared<FilterTable>(*old) :
                         boost::make_shared<FilterTable>());
    fun(*table);

    if (table->empty())
    {
        table = nullptr;
    }

    boost::atomic_exchange(&filter_table,
                           table);
}

typedef struct timeval time_type;

void
log_formatter(const bl::record_view& rec,
              bl::formatting_ostream& strm)
{
    const char* separator  = " -- ";
    {
        const time_type& the_time =
            bl::extract_or_throw<time_type>(timestamp_key, rec);
        static const char* time_format_tpl = "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d:%6.6d %+02.2d%02.2d %s";
        char buf[128];
        struct tm bt;
        localtime_r(&the_time.tv_sec, &bt);
        snprintf(buf, 128, time_format_tpl, 1900 + bt.tm_year,
                 1 + bt.tm_mon,
                 bt.tm_mday,
                 bt.tm_hour,
                 bt.tm_min,
                 bt.tm_sec,
                 the_time.tv_usec,
                 // #warning "check this time zone conversion for negative numbers"
                 bt.tm_gmtoff / 3600,
                 (bt.tm_gmtoff % 3600) / 60,
                 bt.tm_zone);

        strm << (const char*) buf << separator;
    }

    strm << bl::extract_or_throw<Severity>("Severity", rec) << separator;

    strm << bl::extract_or_throw<LOGGER_ATTRIBUTE_ID_TYPE>(LOGGER_ATTRIBUTE_ID,
                                                           rec) << separator ;

    strm << rec[bl::expressions::smessage] << separator ;

    strm << "["
         <<  bl::extract_or_throw< bl::attributes::current_thread_id::value_type>(thread_id_key, rec) << "]";
}

inline time_type
our_time()
{
    struct timeval t;
    gettimeofday(&t, 0);
    return t;
}

bool we_are_deleting = false;

struct SinkDeleter
{
    void
    operator()(bl::sinks::text_file_backend* p)
    {
        we_are_deleting = true;
        try
        {
            delete p;
        }
        catch(...)
        {
        }
        we_are_deleting = false;
    }
};

typedef bl::sinks::file::collector collector_type;

// DONT LOG HERE!!
struct Collector
    : public collector_type
{
    virtual void
    store_file(boost::filesystem::path const& src_path)
    {
        if(we_are_deleting)
        {
            return;
        }

        // USE LOCAL TIME HERE
        // std::cout << "Called with " << src_path << std::endl;
        static const char* time_format_tpl = ".%4.4d-%2.2d-%2.2d";

        boost::posix_time::ptime time(boost::posix_time::second_clock::local_time());

        boost::gregorian::days dd(1);
        time = time - dd;
        boost::gregorian::date date = time.date();

        struct tm bt = boost::gregorian::to_tm(date);

        char buf[128];
        snprintf(buf, 128, time_format_tpl,
                 1900 + bt.tm_year,
                 1 + bt.tm_mon,
                 bt.tm_mday);
        fs::path new_path = src_path.parent_path();
        std::string file_name = src_path.filename().string() + std::string(buf);


        if(fs::exists(new_path / file_name))
        {
            fs::path tmp_file =
                youtils::FileUtils::create_temp_file(new_path /
                                                     (file_name + "_NOTIFY_VOLUMEDRIVER_TEAM_IF_YOU_SEE_THIS_FILE_"));
            fs::rename(src_path, tmp_file);
        }
        else
        {
            fs::rename(src_path, new_path / file_name);
        }
    }

    virtual boost::uintmax_t
    scan_for_files(bl::sinks::file::scan_method,
                   const boost::filesystem::path& ,
                   unsigned* )
    {
        return 0;
    }
};

boost::shared_ptr<bl::sinks::sink> log_sink;

void
setup_console_logging()
{
    typedef bl::sinks::synchronous_sink<bl::sinks::text_ostream_backend> text_sink;

    auto sink(boost::make_shared<text_sink>());
    boost::shared_ptr<std::ostream> stream(&std::clog,
                                           boost::null_deleter());
    sink->locked_backend()->add_stream(stream);
    sink->set_formatter(&log_formatter);

    log_sink = sink;
}

void
setup_file_logging(const fs::path& fname,
                   const LogRotation log_rotation)
{
    typedef bl::sinks::text_file_backend file_backend;
    boost::shared_ptr<file_backend>
        backend(new file_backend(bl::keywords::file_name = fname,
                                 bl::keywords::open_mode = std::ios::out bitor std::ios::app,
                                 bl::keywords::auto_flush = true),
                SinkDeleter());

    //            boost::make_shared< sinks::text_file_backend > ;

    if (T(log_rotation))
    {
        backend->set_time_based_rotation(bl::sinks::file::rotation_at_time_point(0,
                                                                                 0,
                                                                                 0)
                                         // For testing: rotate every 2 seconds.
                                         // bl::sinks::file::rotation_at_time_interval(boost::posix_time::seconds(2))
                                             );

        boost::shared_ptr<collector_type>
            collector(boost::make_shared<Collector>());
        backend->set_file_collector(collector);
    }

    typedef bl::sinks::synchronous_sink<bl::sinks::text_file_backend> sink_t;
    auto sink(boost::make_shared<sink_t>(backend));

    sink->set_formatter(&log_formatter);
    log_sink = sink;
}

}

SeverityLoggerWithName::SeverityLoggerWithName(const std::string& n,
                                               const std::string& /* subsystem */)
    : name(n)
{
    logger_.add_attribute(LOGGER_ATTRIBUTE_ID,
                          boost::log::attributes::constant<LOGGER_ATTRIBUTE_ID_TYPE>(name));
    // logger_.add_attribute(LOGGER_ATTRIBUTE_SUBSYSTEM,
    //                       boost::log::attributes::constant<LOGGER_ATTRIBUTE_SUBSYSTEM_TYPE>(subsystem));
}

void
Logger::setupLogging(const fs::path& log_file_name,
                     const Severity severity,
                     const LogRotation log_rotation)
{
    boost::lock_guard<decltype(update_lock)> g(update_lock);

    global_severity = severity;

    if (log_sink != nullptr)
    {
        // logging was initialized before
    }
    else
    {
        bl::core::get()->add_global_attribute(thread_id_key,
                                              bl::attributes::current_thread_id());
        bl::core::get()->add_global_attribute(timestamp_key,
                                              bl::attributes::function<time_t>(&our_time));

        if (log_file_name.empty())
        {
            setup_console_logging();
        }
        else
        {
            setup_file_logging(log_file_name,
                               log_rotation);
        }

        bl::core::get()->add_sink(log_sink);
    }

    bl::core::get()->set_exception_handler(bl::make_exception_suppressor());
}

void
Logger::teardownLogging()
{
    // flushing & closing here??
    update_filter_table([&](FilterTable& table)
                        {
                            disableLogging();
                            bl::core::get()->remove_all_sinks();
                            table.clear();
                            log_sink = nullptr;
                        });
}

void
Logger::generalLogging(Severity severity)
{
    if (loggingEnabled())
    {
        global_severity.store(severity);
    }
    else
    {
        throw LoggerNotConfiguredException("Logging is disabled");
    }
}

Severity
Logger::generalLogging()
{
    if (loggingEnabled())
    {
        return global_severity.load();
    }
    else
    {
        throw LoggerNotConfiguredException("Logging is disabled");
    }
}

void
Logger::add_filter(const std::string& filter_name,
                   const Severity severity)
{
    if (not loggingEnabled())
    {
        throw LoggerNotConfiguredException("Logging is disabled");
    }

    update_filter_table([&](FilterTable& table)
                        {
                            table[filter_name] = severity;
                        });
}

void
Logger::remove_filter(const std::string& name)
{
    if (not loggingEnabled())
    {
        throw LoggerNotConfiguredException("Logging is disabled");
    }

    update_filter_table([&](FilterTable& table)
                        {
                            table.erase(name);
                        });
}

void
Logger::remove_all_filters()
{
    if (not loggingEnabled())
    {
        throw LoggerNotConfiguredException("Logging is disabled");
    }

    update_filter_table([&](FilterTable& table)
                        {
                            table.clear();
                        });
}

void
Logger::all_filters(std::vector<std::pair<std::string, youtils::Severity> >& out)
{
    if (not loggingEnabled())
    {
        throw LoggerNotConfiguredException("Logging is disabled");
    }

    FilterTablePtr table(boost::atomic_load(&filter_table));
    if (table != nullptr)
    {
        for (const auto& p : *table)
        {
            out.push_back(p);
        }
    }
}

bool
Logger::filter(const std::string& name,
               const Severity sev)
{
    bool res = false;

    if (loggingEnabled())
    {
        if (sev >= global_severity)
        {
            res = true;
        }

        FilterTablePtr table(boost::atomic_load(&filter_table));
        if (table != nullptr)
        {
            const auto it = table->find(name);
            if (it != table->end())
            {
                res = sev >= it->second;
            }
        }
    }

    return res;
}

void
Logger::disableLogging()
{
    bl::core::get()->set_logging_enabled(false);
}

void
Logger::enableLogging()
{
    if (log_sink == nullptr)
    {
        throw LoggerNotConfiguredException("Logging is not configured");
    }

    bl::core::get()->set_logging_enabled(true);
}

bool
Logger::loggingEnabled()
{
    return bl::core::get()->get_logging_enabled();
}

namespace
{
inline const char*
severity_to_string(Severity& severity)
{
    switch(severity)
    {
    case Severity::trace:
        return "trace";
    case Severity::debug:
        return "debug";
    case Severity::periodic:
        return "periodic";
    case Severity::info:
        return "info";
    case Severity::warning:
        return "warning";
    case Severity::error:
        return "error";
    case Severity::fatal:
        return "fatal";
    case Severity::notification:
        return "notice";
    }
    UNREACHABLE;
}

}

std::ostream&
operator<<(std::ostream& os, Severity severity)
{
    return os << severity_to_string(severity);
}

std::istream&
operator>>(std::istream& is,
           Severity& severity)
{
    // We might want to optimize this
    std::string str;
    is >> str;

    if(str == "trace")
    {
        severity = Severity::trace;
    }
    else if(str == "debug")
    {
        severity = Severity::debug;
    }
    else if(str == "periodic")
    {
        severity = Severity::periodic;
    }
    else if(str == "info")
    {
        severity = Severity::info;
    }
    else if(str == "warning")
    {
        severity =  Severity::warning;
    }
    else if(str == "error")
    {
        severity = Severity::error;
    }
    else if (str == "fatal")
    {
        severity = Severity::fatal;
    }
    else if (str == "notice")
    {
        severity = Severity::notification;
    }
    else
    {
        is.setstate(std::ios_base::failbit);
    }

    return is;
}

}
