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

#ifndef YT_REDIS_URL_H_
#define YT_REDIS_URL_H_

#include "Url.h"

namespace youtils
{

struct RedisUrl
    : public Url<RedisUrl>
{
    using Url<RedisUrl>::Url;
};

} //namespace youtils


#endif //YT_REDIS_URL_H_
