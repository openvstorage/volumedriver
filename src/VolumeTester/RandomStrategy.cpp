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

#include "RandomStrategy.h"
#include "VTException.h"
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


RandomStrategy::RandomStrategy()
{
    mode_t mode = 0;
    fd = open("/dev/urandom",O_RDONLY,mode);
    if(fd < 0)
    {
        throw VTException("Could not open file");
    }
    ssize_t red = read(fd,buf,Cblocksize);
    if(red != static_cast<const ssize_t>(Cblocksize))
    {
        throw VTException("Could not read enough data from /dev/urandom");
    }

}


RandomStrategy::~RandomStrategy()
{
    close(fd);
}



void
RandomStrategy::operator()(unsigned char* buf1,
                           unsigned long blocksize) const
{
    assert(blocksize <= Cblocksize);
    memcpy(buf1,buf,blocksize);
}

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
