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

#ifndef ARAKOON_INI_PARSER_H_
#define ARAKOON_INI_PARSER_H_

#include "ArakoonNodeConfig.h"
#include "IOException.h"
#include "Logging.h"

#include <iosfwd>

namespace arakoon
{

class IniParser
{
public:
    explicit IniParser(std::istream&);

    ~IniParser() = default;

    IniParser(const IniParser&) = delete;

    IniParser&
    operator=(const IniParser&) = delete;

    const ClusterID&
    cluster_id() const
    {
        return cluster_id_;
    }

    const std::vector<ArakoonNodeConfig>&
    node_configs() const
    {
        return node_configs_;
    }

private:
    DECLARE_LOGGER("ArakoonIniParser");

    ClusterID cluster_id_;
    std::vector<ArakoonNodeConfig> node_configs_;
};

}

#endif // !ARAKOON_INI_PARSER_H_
