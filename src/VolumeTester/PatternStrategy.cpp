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

#include "PatternStrategy.h"

const std::string
PatternStrategy::default_pat_("darling!");

PatternStrategy::PatternStrategy(const std::string& pat)
    : pat_(pat.empty() ? default_pat_ : pat)
    , sz_(pat_.size())
{}

void
PatternStrategy::operator()(unsigned char* buf1,
                            unsigned long blocksize) const
{
  for(size_t i = 0; i < blocksize; i++)
  {
    buf1[i] = pat_[i%sz_];
  }
}

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
