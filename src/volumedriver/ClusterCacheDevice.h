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
BOOST_CLASS_VERSION(volumedriver::ClusterCacheDeviceManager, 1);

#endif  // READ_CACHE_DEVICE_MANAGER_H

// Local Variables: **
// mode: c++ **
// End: **
