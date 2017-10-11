// Copyright (C) 2017 iNuron NV
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

#ifndef VD_VOLUME_CONFIG_PERSISTOR_H_
#define VD_VOLUME_CONFIG_PERSISTOR_H_

#include <iosfwd>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>

#include <youtils/Logging.h>

namespace backend
{
class BackendInterface;
class Condition;
}

namespace volumedriver
{

class VolumeConfig;

struct VolumeConfigPersistor
{
    static void
    load(std::istream&,
         VolumeConfig&);

    static void
    load(const boost::filesystem::path&,
         VolumeConfig&);

    static void
    load(backend::BackendInterface&,
         VolumeConfig&);

    static VolumeConfig
    load(backend::BackendInterface&);

    static void
    save(backend::BackendInterface&,
         const VolumeConfig&,
         const boost::shared_ptr<backend::Condition>& = nullptr);

    DECLARE_LOGGER("VolumeConfigPersistor");
};

}

#endif // !VD_VOLUME_CONFIG_PERSISTOR_H_
