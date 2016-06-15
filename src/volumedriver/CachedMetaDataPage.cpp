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

#include "CachedMetaDataPage.h"

#include <youtils/System.h>

namespace volumedriver
{

namespace yt = youtils;

const uint8_t CachePage::page_bits_ =
    yt::System::get_env_with_default<uint32_t>("METADATASTORE_BITS", 8);

}

// Local Variables: **
// mode: c++ **
// End: **
