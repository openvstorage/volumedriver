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

#include "NormalizedSCOAccessData.h"
#include "SCOAccessData.h"
#include <youtils/Assert.h>

namespace scrubbing
{
using namespace volumedriver;

NormalizedSCOAccessData::NormalizedSCOAccessData(const BackendInterface& bi_in)
{
    try
    {
        BackendInterfacePtr bi(bi_in.cloneWithNewNamespace(bi_in.getNS()));
        SCOAccessDataPersistor sadp(std::move(bi));
        SCOAccessDataPtr sad(sadp.pull());

        const SCOAccessData::VectorType& vec = sad->getVector();
        float total = 0;

        for(SCOAccessData::VectorType::const_iterator it = vec.begin();
            it != vec.end();
            ++it)
        {
            VERIFY(it->second >= 0);
            total += it->second;

        }

        if(total > 0)
        {
            for(SCOAccessData::VectorType::const_iterator it = vec.begin();
                it != vec.end();
                ++it)
            {
                insert(std::make_pair(it->first, it->second / total));
            }
        }
    }
    catch(...)
    {
        // No problem each sco has 0 access probability
        LOG_WARN("No SCO Access Data for this scrubbing run found");
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
