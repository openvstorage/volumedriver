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

#include "TLogToolCut.h"

namespace volumedriver
{

namespace python
{

namespace fs = boost::filesystem;
namespace yt = youtils;

TLogToolCut::TLogToolCut(const vd::TLog& tlog)
    : tlog_(tlog)
{}

bool
TLogToolCut::writtenToBackend() const
{
    return tlog_.writtenToBackend();
}

std::string
TLogToolCut::getName() const
{
    return tlog_.getName();
}

std::string
TLogToolCut::getUUID() const
{
    return static_cast<yt::UUID>(tlog_.id()).str();
}

bool
TLogToolCut::isTLogString(const std::string& in)
{
    return vd::TLog::isTLogString(in);
}

std::string
TLogToolCut::getUUIDFromTLogName(const std::string& tlogName)
{
    return static_cast<yt::UUID>(boost::lexical_cast<vd::TLogId>(tlogName)).str();
}

std::string
TLogToolCut::str() const
{
    std::stringstream ss;
    ss << "name: " << getName() << std::endl;
    ss << "writtenToBackend: " << writtenToBackend() << std::endl;
    ss << "uuid: " << getUUID() << std::endl;
    return ss.str();
}

std::string
TLogToolCut::repr() const
{
    return std::string("< TLog \n") + str() + "\n>";
}

}

}
// Local Variables: **
// mode: c++ **
// End: **
