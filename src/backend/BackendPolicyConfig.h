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

#ifndef BACKEND_POLICY_CONFIG_H_
#define BACKEND_POLICY_CONFIG_H_

#include <stdint.h>
#include <vector>
#include <string>
namespace backend
{

enum class SafetyStrategy
{
    RepairSpread = 1,
    DynamicSafety = 2
};

struct BackendPolicyConfig

{

    BackendPolicyConfig(uint64_t min_superblock_size,
                        uint64_t max_superblock_size,
                        int spread_width,
                        int safety,
                        int n_messages,
                        const std::vector<bool>& hierarchy_rules,
                        const std::vector<SafetyStrategy>& safety_strategies)
        : min_superblock_size_(min_superblock_size)
        , max_superblock_size_(max_superblock_size)
        , spread_width_(spread_width)
        , safety_(safety)
        , n_messages_(n_messages)
        , hierarchy_rules_(hierarchy_rules)
        , safety_strategies_(safety_strategies)
    {}

    const uint64_t min_superblock_size_;
    const uint64_t max_superblock_size_;
    int spread_width_;
    int safety_;
    int n_messages_;
    std::vector<bool> hierarchy_rules_;
    std::vector<SafetyStrategy> safety_strategies_;
};

typedef std::string BackendPolicyConfigId;

}
#endif // !BACKEND_POLICY_CONFIG_H_

// Local Variables: **
// mode: c++ **
// End: **
