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

#ifndef _SCRIPT_FILE_H_
#define _SCRIPT_FILE_H_

#include <vector>
#include <string>
#include "FileUtils.h"
#include "Assert.h"
#include <boost/filesystem/fstream.hpp>
#include <sys/stat.h>

namespace youtils
{
struct ScriptFile
{
    DECLARE_LOGGER("ScriptFile");

    ScriptFile(const fs::path& p,
               const std::vector<std::string>& lines)
    {
        boost::filesystem::ofstream os(p);
        os << "#! /bin/bash" << std::endl;
        os << "set -eux" << std::endl;
        for(const std::string& line : lines)
        {
            os << line << std::endl;
        }
        int res = chmod(p.string().c_str(),0777);
        VERIFY(res == 0);
    }
};

}

#endif // _SCRIPT_FILE_H_
