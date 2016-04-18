// Copyright 2016 iNuron NV
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

#ifndef NETWORK_XIO_WORK_H_
#define NETWORK_XIO_WORK_H_

#include <functional>
#include <libxio.h>

namespace volumedriverfs
{

struct Work;

typedef std::function<void(Work*)> workitem_func_t;

struct Work
{
    workitem_func_t func;
    void *obj;
};

} //namespace

#endif //NETWORK_XIO_WORK_H_
