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
