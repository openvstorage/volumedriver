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
