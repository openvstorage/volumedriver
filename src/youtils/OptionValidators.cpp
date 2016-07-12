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

#include "EtcdUrl.h"
#include "OptionValidators.h"

namespace youtils
{

using namespace boost::program_options;
namespace fs = ::boost::filesystem;

void
validate(boost::any& v,
         const std::vector<std::string>& values,
         ExistingFile* /* out */,
         int)
{
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

void
validate(boost::any& v,
         const std::vector<std::string>& values,
         MaybeConfigLocation* /* out */,
         int)
{
    validators::check_first_occurrence(v);
    const std::string& s = validators::get_single_string(values);
    if(s.empty())
    {
        throw validation_error(validation_error::invalid_option_value);
    }

    if (EtcdUrl::is_one(s))
    {
        v = boost::any(MaybeConfigLocation(ConfigLocation(s)));
    }
    else
    {
        const fs::path p(s);
        if(fs::exists(p))
        {
            v = boost::any(MaybeConfigLocation(ConfigLocation(s)));
        }
        else
        {
            throw validation_error(validation_error::invalid_option_value);
        }
    }
}

}
