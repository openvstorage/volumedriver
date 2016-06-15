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
