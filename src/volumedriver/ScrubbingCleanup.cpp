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

#include "ScrubbingCleanup.h"

#include <iostream>

namespace volumedriver
{

std::ostream&
operator<<(std::ostream& os,
           ScrubbingCleanup s)
{
    switch (s)
    {
    case ScrubbingCleanup::Never:
        os << "Never";
        break;
    case ScrubbingCleanup::OnError:
        os << "OnError";
        break;
    case ScrubbingCleanup::OnSuccess:
        os << "OnSuccess";
        break;
    case ScrubbingCleanup::Always:
        os << "Always";
        break;
    }

    return os;
}

}
