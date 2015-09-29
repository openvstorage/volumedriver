// Copyright 2015 Open vStorage NV
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

#ifndef OPTION_VALIDATORS_H_
#define OPTION_VALIDATORS_H_
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/optional.hpp>
namespace youtils
{

struct ExistingFile : boost::optional<boost::filesystem::path>
{
    ExistingFile(boost::filesystem::path& pth)
        : boost::optional<boost::filesystem::path>(pth)
    {}

    ExistingFile()
    {}

};

void
validate(boost::any& v,
         const std::vector<std::string>& values,
         ExistingFile* out,
         int);

}


#endif // OPTION_VALIDATORS_H_
// Local Variables: **
// mode: c++ **
// End: **
