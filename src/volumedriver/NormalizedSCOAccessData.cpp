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
