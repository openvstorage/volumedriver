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

#include "VolumeConfig.h"
#include "VolumeConfigPersistor.h"

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/filesystem/fstream.hpp>

#include <youtils/ArchiveTraits.h>
#include <youtils/MultiArchivePersistor.h>
#include <youtils/Serialization.h>

#include <backend/BackendInterface.h>

namespace volumedriver
{

namespace ba = boost::archive;
namespace be = backend;
namespace fs = boost::filesystem;
namespace yt = youtils;

using IArchiveList = LOKI_TYPELIST_2(ba::text_iarchive,
                                     ba::xml_iarchive);

void
VolumeConfigPersistor::load(std::istream& is,
                            VolumeConfig& cfg)
{
    yt::MultiArchivePersistor<IArchiveList>::load(is,
                                                  VolumeConfig::config_backend_name.c_str(),
                                                  cfg);
}

void
VolumeConfigPersistor::load(const fs::path& p,
                            VolumeConfig& cfg)
{
    fs::ifstream ifs(p);
    load(ifs,
         cfg);
}

void
VolumeConfigPersistor::load(be::BackendInterface& bi,
                            VolumeConfig& cfg)
{
    bi.fillObject(cfg,
                  VolumeConfig::config_backend_name,
                  InsistOnLatestVersion::T,
                  [](std::istream& is,
                     VolumeConfig& cfg)
                  {
                      yt::MultiArchivePersistor<IArchiveList>::load(is,
                                                                    VolumeConfig::config_backend_name.c_str(),
                                                                    cfg);
                  });
}

VolumeConfig
VolumeConfigPersistor::load(be::BackendInterface& bi)
{
    VolumeConfig cfg;
    load(bi,
         cfg);

    return cfg;
}

void
VolumeConfigPersistor::save(be::BackendInterface& bi,
                            const VolumeConfig& cfg,
                            const boost::shared_ptr<be::Condition>& cond)
{
    bi.writeObject(cfg,
                   VolumeConfig::config_backend_name,
                   [](std::ostream& os,
                      const VolumeConfig& cfg)
                   {
                      yt::MultiArchivePersistor<IArchiveList>::save(os,
                                                                    VolumeConfig::config_backend_name.c_str(),
                                                                    cfg);
                      os.flush();
                      if (not os.good())
                      {
                          LOG_ERROR("failed to flush output stream");
                          throw youtils::SerializationFlushException();
                      }
                   },
                   OverwriteObject::T,
                   cond);
}

}
