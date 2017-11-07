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

#ifndef SCO_ACCESS_DATA_INFO_H_
#define SCO_ACCESS_DATA_INFO_H_

#include <boost/python/list.hpp>
#include <boost/filesystem.hpp>

#include "../SCOAccessData.h"


namespace volumedriver
{

namespace python
{

namespace bpy = boost::python;
namespace fs = boost::filesystem;
namespace vd = volumedriver;

class SCOAccessDataInfo
{
public:
    explicit SCOAccessDataInfo(const fs::path& p);

    SCOAccessDataInfo(const SCOAccessDataInfo&) = delete;

    SCOAccessDataInfo&
    operator=(const SCOAccessDataInfo&) = delete;

    std::string
    getNamespace() const;

    float
    getReadActivity() const;

    bpy::list
    getEntries() const;

private:
    DECLARE_LOGGER("SCOAccessDataInfo");

    vd::SCOAccessDataPtr sad_;
};

}

}

#endif //! SCO_ACCESS_DATA_INFO_H_

// Local Variables: **
// mode: c++ **
// End: **
