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

#ifndef VD_SCRUB_ID_H_
#define VD_SCRUB_ID_H_

#include <boost/optional.hpp>

#include <youtils/OurStrongTypedef.h>
#include <youtils/UUID.h>

OUR_STRONG_NON_ARITHMETIC_TYPEDEF(youtils::UUID,
                                  ScrubId,
                                  volumedriver);

namespace volumedriver
{

using MaybeScrubId = boost::optional<ScrubId>;

}

#endif // !VD_SCRUB_ID_H_
