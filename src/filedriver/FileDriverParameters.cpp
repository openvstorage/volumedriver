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

#include "FileDriverParameters.h"

namespace initialized_params
{

const char file_driver_component_name[] = "file_driver";

DEFINE_INITIALIZED_PARAM(fd_cache_path,
                         file_driver_component_name,
                         "fd_cache_path",
                         "cache for filedriver objects",
                         ShowDocumentation::T);

DEFINE_INITIALIZED_PARAM(fd_namespace,
                         file_driver_component_name,
                         "fd_namespace",
                         "backend namespace to use for filedriver objects",
                         ShowDocumentation::T);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fd_extent_cache_capacity,
                                      file_driver_component_name,
                                      "fd_extent_cache_capacity",
                                      "number of extents the extent cache can hold",
                                      ShowDocumentation::T,
                                      1024);
}
