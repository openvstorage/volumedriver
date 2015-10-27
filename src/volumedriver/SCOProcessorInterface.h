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

#ifndef VD_SCOPROCESSORINTERFACE_H
#define VD_SCOPROCESSORINTERFACE_H

#include "ClusterLocation.h"

namespace volumedriver
{

using SCOProcessorFun = std::function<void(ClusterLocation,
                       uint64_t /* lba */,
                       const uint8_t* /* buf */,
                       size_t /* bufsize */)>;

#define SCOPROCESSORFUN(ttype, mmethod, inst) SCOProcessorFun(std::bind(&ttype::mmethod, inst, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4))

} // namespace volumedriver

#endif // VD_SCOPROCESSORINTERFACE_H
