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

#ifndef ERROR_RULES_H_
#define ERROR_RULES_H_

#include <memory>

#include <boost/regex.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include "FileSystemCall.h"
namespace bl = boost::logic;

namespace fawltyfs
{

typedef uint32_t rule_id;

// sole purpose is to aggregate common fields, hence a non-public constructor.
class ErrorBaseRule
{
public:
    ErrorBaseRule(const ErrorBaseRule&) = delete;

    ErrorBaseRule&
    operator=(const ErrorBaseRule&) = delete;

    const rule_id id;
    const boost::regex path_regex;
    const std::set<FileSystemCall> calls_ ;
    const uint32_t ramp_up;
    const uint32_t duration;

    bl::tribool
    match(const std::string& path,
          const FileSystemCall call);

    bool
    expired() const
    {
        return expired_;
    }

    uint64_t
    matches() const
    {
        return matches_;
    }

protected:
    ErrorBaseRule(rule_id id,
                  const std::string& path_regex,
                  const std::set<FileSystemCall>&  calls,
                  uint64_t ramp_up,
                  uint64_t duration);

    uint64_t matches_;
    bool expired_;
};

class DelayRule
    : public ErrorBaseRule
{
public:
    DelayRule(rule_id id,
              const std::string& path_regex,
              const std::set<FileSystemCall>& calls,
              uint32_t ramp_up,
              uint32_t duration,
              uint32_t idelay_usecs);

    DelayRule(const DelayRule&) = delete;

    DelayRule&
    operator=(const DelayRule&) = delete;

    const uint32_t delay_usecs;

    void
    abort();

    void
    operator()() const;
private:
    mutable boost::mutex lock_;
    mutable boost::condition_variable condvar_;
};

typedef std::shared_ptr<DelayRule> DelayRulePtr;
typedef std::list<DelayRulePtr> delay_rules_list_t;

struct FailureRule
    : public ErrorBaseRule
{
    FailureRule(rule_id id,
                const std::string& path_regex,
                const std::set<FileSystemCall>& calls,
                uint32_t ramp_up,
                uint32_t duration,
                int errcode);

    FailureRule(const FailureRule&) = delete;

    FailureRule&
    operator=(const FailureRule&) = delete;

    const int errcode;

    int operator()() const;
};

typedef std::shared_ptr<FailureRule> FailureRulePtr;
typedef std::list<FailureRulePtr> failure_rules_list_t;

}

#endif // !ERROR_RULES_H_

// Local Variables: **
// compile-command: "scons -D" **
// mode: c++ **
// End: **
