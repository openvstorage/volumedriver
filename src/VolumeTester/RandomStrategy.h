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

#ifndef RANDOM_STRATEGY_H_
#define RANDOM_STRATEGY_H_
#include "Strategy.h"
#include <youtils/Logging.h>

static const size_t Cblocksize = 4 * 4096;

class RandomStrategy : public Strategy
{
  public:
    RandomStrategy();

    virtual ~RandomStrategy();

    virtual void
    operator()(unsigned char* buf1,
               unsigned long blocksize) const;

  private:
    DECLARE_LOGGER("RandomStrategy");
    int fd;
    char buf[Cblocksize];
};

#endif // RANDOM_STRATEGY_H_

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
