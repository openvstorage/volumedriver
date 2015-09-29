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
