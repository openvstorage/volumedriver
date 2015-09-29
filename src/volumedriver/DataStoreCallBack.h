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

#ifndef DATASTORECALLBACK_H_
#define DATASTORECALLBACK_H_

#include "Types.h"
#include <youtils/CheckSum.h>
#include "SCO.h"

namespace volumedriver {

class DataStoreCallBack
{
public:
    virtual ~DataStoreCallBack() {}

    virtual void
    writtenToBackend(SCO n) = 0;

    virtual CachedSCOPtr
    getSCO(SCO,
           BackendInterfacePtr,
           const CheckSum* cs = 0) = 0;

    virtual void
    reportIOError(SCO sconame,
                  const char* errmsg) = 0;
};

}

#endif /* DATASTORECALLBACK_H_ */

// Local Variables: **
// mode: c++ **
// End: **
