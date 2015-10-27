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

#ifndef METADATA_STORE_DEBUG_H
#define METADATA_STORE_DEBUG_H

#include "MetaDataStoreInterface.h"

#include <ostream>

namespace volumedriver
{
void
dump_mdstore(MetaDataStoreInterface& mdstore,
             ClusterAddress max_address,
             std::ostream& out = std::cout,
             bool with_hash = false);
}

#endif // METADATA_STORE_DEBUG_H
