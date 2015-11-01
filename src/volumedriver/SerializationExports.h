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

#ifndef VD_SERIALIZATION_EXPORTS_H_
#define VD_SERIALIZATION_EXPORTS_H_

#include "MetaDataBackendConfig.h"

#include <boost/serialization/export.hpp>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>

#include <youtils/Assert.h>

TODO("AR: investigate and fix this!");
// include this header into the respective main() files - including somewhere in a file
// that will end up in a library is not sufficient!

BOOST_CLASS_EXPORT_IMPLEMENT(volumedriver::MetaDataBackendConfig);
BOOST_CLASS_EXPORT_IMPLEMENT(volumedriver::TCBTMetaDataBackendConfig);
BOOST_CLASS_EXPORT_IMPLEMENT(volumedriver::RocksDBMetaDataBackendConfig);
BOOST_CLASS_EXPORT_IMPLEMENT(volumedriver::ArakoonMetaDataBackendConfig);
BOOST_CLASS_EXPORT_IMPLEMENT(volumedriver::MDSMetaDataBackendConfig);

#endif // !VD_SERIALIZATION_EXPORTS_H_
