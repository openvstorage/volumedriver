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

#ifndef FILE_DRIVER_COMPONENT_H_
#define FILE_DRIVER_COMPONENT_H_

#include <string>

#include <youtils/InitializedParam.h>

namespace initialized_params
{

extern const char file_driver_component_name[];

DECLARE_INITIALIZED_PARAM(fd_cache_path, std::string);
DECLARE_INITIALIZED_PARAM(fd_namespace, std::string);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(fd_extent_cache_capacity, uint32_t);

}

#endif // !FILE_DRIVER_COMPONENT_H_
