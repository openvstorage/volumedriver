// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
