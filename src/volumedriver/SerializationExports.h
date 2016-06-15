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
