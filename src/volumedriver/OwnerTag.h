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

#ifndef VD_OWNER_TAG_H_
#define VD_OWNER_TAG_H_

#include <youtils/OurStrongTypedef.h>

// Support for moving volumes between instances: a changed tag indicates changed
// ownership which could necessitate cache invalidations etc.
// OwnerTag(0) is reserved: it is used internally by volumedriver for backward compat
// purposes but callers must not use it.
OUR_STRONG_NON_ARITHMETIC_TYPEDEF(uint64_t, OwnerTag, volumedriver);

#endif // !VD_VOLUME_GENERATION_H_
