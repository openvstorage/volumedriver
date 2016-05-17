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

#ifndef SCRUBBING_SCO_DATA_H
#define SCRUBBING_SCO_DATA_H

#include "SCO.h"
#include "Types.h"

#include <iosfwd>

namespace scrubbing
{

class ScrubbingSCOData
{
    friend
    std::ostream&
    operator<<(std::ostream&, const ScrubbingSCOData&);
public:
    enum class State : uint16_t
    {
        Unknown,
        Scrubbed,
        NotScrubbed,
        Reused,
    };

    explicit ScrubbingSCOData(volumedriver::SCO sconame);

    volumedriver::SCO sconame_;
    uint16_t usageCount;
    uint16_t size;
    State state;

};

std::ostream&
operator<<(std::ostream&, const ScrubbingSCOData::State&);

// std::ostream&
// operator<<(std::ostream&, const ScrubbingSCOData&);

typedef std::vector<ScrubbingSCOData> ScrubbingSCODataVector;

std::ostream&
operator<<(std::ostream&, const ScrubbingSCODataVector&);
}

#endif // SCRUBBING_SCO_DATA_H

// Local Variables: **
// mode : c++ **
// End: **
