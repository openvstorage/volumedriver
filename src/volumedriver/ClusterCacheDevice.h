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

#ifndef READ_CACHE_DEVICE_MANAGER_H
#define READ_CACHE_DEVICE_MANAGER_H

#include "ClusterCacheDiskStore.h"
#include "ClusterCacheDeviceT.h"

namespace volumedriver
{

typedef ClusterCacheDeviceT<ClusterCacheDiskStore> ClusterCacheDevice;
typedef ClusterCacheDeviceManagerT<ClusterCacheDevice> ClusterCacheDeviceManager;

}

BOOST_CLASS_VERSION(volumedriver::ClusterCacheDeviceManager::ClusterCacheDeviceInMapper, 0);
BOOST_CLASS_VERSION(volumedriver::ClusterCacheDeviceManager::ClusterCacheDeviceOutMapper, 0)
BOOST_CLASS_VERSION(volumedriver::ClusterCacheDevice, 0);
BOOST_CLASS_VERSION(volumedriver::ClusterCacheDeviceManager, 2);

#endif  // READ_CACHE_DEVICE_MANAGER_H

// Local Variables: **
// mode: c++ **
// End: **
