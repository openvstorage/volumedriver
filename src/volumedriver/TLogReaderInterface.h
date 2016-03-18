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

#ifndef TLOGREADERINTERFACE_H
#define TLOGREADERINTERFACE_H
#include "Entry.h"
#include <vector>
#include "SCO.h"
#include <youtils/IOException.h>

namespace volumedriver
{
BOOLEAN_ENUM(KeepRunningCRC);
BOOLEAN_ENUM(CheckSCOCRC);

class TLogReaderInterface
{
public:
    virtual ~TLogReaderInterface() = default;

    virtual const Entry*
    nextAny() = 0;

    ClusterLocation
    nextClusterLocation();

    const Entry*
    nextLocation();

    void
    SCONames(std::vector<SCO>& out);

    void
    SCONamesAndChecksums(std::map<SCO, CheckSum>& out_checksums);

    // Revisit this: would be much nicer if we could specify the member function to be called
    // and default it to processEntry but for some reason that doesn't work.
    template<typename T>
    void
    for_each(T& t)
    {
        const Entry* e = 0;
        while((e = nextAny()))
        {
            t.processEntry(e);
        }
    }

    using Ptr = std::unique_ptr<TLogReaderInterface>;
};

}

#endif // TLOGREADERINTERFACE_H



// Local Variables: **
// mode: c++ **
// End: **
