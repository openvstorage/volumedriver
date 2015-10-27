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

#ifndef _LITERALS_H_
#define _LITERALS_H_

#define TEST_NAME "test"
#define TEST_KIND "context"

#define OBJECT_NAME "fawlty"
#define OBJECT_KIND "Object"
#include "fawlty.hh"
#include "../FileSystem.h"
fawltyfs::FileSystemCall
fromCorba(const Fawlty::FileSystemCall in);

Fawlty::FileSystemCall
toCorba(fawltyfs::FileSystemCall in);

std::set<fawltyfs::FileSystemCall>
fromCorba(const Fawlty::FileSystemCalls& calls);

Fawlty::FileSystemCalls
toCorba(const std::set<fawltyfs::FileSystemCall>& calls);

#endif // _LITERALS_H_
