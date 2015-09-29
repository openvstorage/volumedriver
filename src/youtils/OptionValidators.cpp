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

#include "OptionValidators.h"

namespace youtils
{
namespace fs = ::boost::filesystem;

void
validate(boost::any& v,
         const std::vector<std::string>& values,
         ExistingFile* /* out */,
         int)
{
    using namespace boost::program_options;
    validators::check_first_occurrence(v);
    const std::string& s = validators::get_single_string(values);
    if(s.empty())
    {
        throw validation_error(validation_error::invalid_option_value);
    }


    fs::path file(s);

    if(fs::exists(file))
    {
        v = boost::any(ExistingFile(file));
    }
    else
    {
        throw validation_error(validation_error::invalid_option_value);
    }
}
}
