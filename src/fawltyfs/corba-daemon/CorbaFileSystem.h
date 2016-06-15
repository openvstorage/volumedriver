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

#ifndef CORBA_FILESYSTEM_H_
#define CORBA_FILESYSTEM_H_
#include "../FileSystem.h"
#include "fawlty.hh"
#include <youtils/Logging.h>
class CorbaFileSystem : private fawltyfs::FileSystem
{
public:
    CorbaFileSystem(const std::string& frontend,
                    const std::string& backend,
                    const char* fsname)
        : fawltyfs::FileSystem(frontend,
                             backend,
                             {"-f", "-s"},
                             fsname)
    {}

    DECLARE_LOGGER("CorbaFileSystem");

    void
    mount();

    void
    umount();

    void
    addDelayRule(const int rule_id,
                 const char* path_regex,
                 const Fawlty::FileSystemCalls& calls,
                 const int ramp_up,
                 const int duration,
                 const int idelay_usecs);

    void
    removeDelayRule(const int rule_id);

    Fawlty::DelayRules*
    listDelayRules();


    void
    addFailureRule(const int rule_id,
                   const char* path_regex,
                   const Fawlty::FileSystemCalls& calls,
                   const int ramp_up,
                   const int duration,
                   const int errcode);


    void
    removeFailureRule(const int rule_id);

    Fawlty::FailureRules*
    listFailureRules();


    void
    addDirectIORule(const int rule_id,
                    const char* path_regex,
                    const bool direct_io);

    void
    removeDirectIORule(const int rule_id);

    Fawlty::DirectIORules*
    listDirectIORules();

    void
    f(std::shared_ptr<int>)
    {}

};


#endif // CORBA_FILESYSTEM_H_
