// Copyright (C) 2016 iNuron NV
//
// This file is part of Open vStorage Open Source Edition (OSE),
// as available from
//
//      http://www.openvstorage.org and
//      http://www.openvstorage.com.
//
// This file is free software; you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License v3 (GNU AGPLv3)
// as published by the Free Software Foundation, in version 3 as it comes in
// the LICENSE.txt file of the Open vStorage OSE distribution.
// Open vStorage is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY of any kind.

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
