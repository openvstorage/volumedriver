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

#include "ErrorRules.h"

namespace fawltyfs
{

ErrorBaseRule::ErrorBaseRule(rule_id iid,
                             const std::string& ipath_regex,
                             const std::set<FileSystemCall>& calls,
                             uint64_t iramp_up,
                             uint64_t iduration)
    : id(iid)
    , path_regex(ipath_regex)
    , calls_(calls)
    , ramp_up(iramp_up)
    , duration(iduration)
    , matches_(0)
    , expired_(false)
{}

bl::tribool
ErrorBaseRule::match(const std::string& path,
                     const FileSystemCall call)
{
    if (regex_match(path, path_regex) and
        calls_.count(call) == 1 and
        not expired_)
    {
        ++matches_;
        if (matches_ <= ramp_up)
        {
            return bl::indeterminate;
        }
        else if (matches_ == ramp_up + duration)
        {
            expired_ = true;
        }
        return true;
    }
    else
    {
        return false;
    }
}

DelayRule::DelayRule(rule_id id,
                     const std::string& path_regex,
                     const std::set<FileSystemCall>& call_regex,
                     uint32_t ramp_up,
                     uint32_t duration,
                     uint32_t idelay_usecs)
    : ErrorBaseRule(id, path_regex, call_regex, ramp_up, duration)
    , delay_usecs(idelay_usecs)
{}

void
DelayRule::operator()() const
{
    boost::unique_lock<boost::mutex> l(lock_);
    condvar_.timed_wait(l,
                        boost::posix_time::microseconds(delay_usecs));
}

void
DelayRule::abort()
{
    boost::lock_guard<boost::mutex> g(lock_);
    condvar_.notify_all();
}

FailureRule::FailureRule(rule_id id,
                         const std::string& path_regex,
                         const std::set<FileSystemCall>& call_regex,
                         uint32_t ramp_up,
                         uint32_t duration,
                         int ierrcode)
    : ErrorBaseRule(id, path_regex, call_regex, ramp_up, duration)
    , errcode(ierrcode)
{}

int
FailureRule::operator()() const
{
    return errcode;
}

}

// Local Variables: **
// compile-command: "scons -D -j 4" **
// mode: c++ **
// End: **
