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
