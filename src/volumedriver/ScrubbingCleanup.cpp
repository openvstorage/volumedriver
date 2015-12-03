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
