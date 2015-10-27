// Copyright 2015 iNuron NV
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

#ifndef VD_META_DATA_STORE_BUILDER_H_
#define VD_META_DATA_STORE_BUILDER_H_

#include "MetaDataStoreInterface.h"
#include "NSIDMap.h"

#include <youtils/Logging.h>

#include <boost/filesystem.hpp>

#include <backend/BackendInterface.h>

namespace volumedriver
{

BOOLEAN_ENUM(CheckScrubId);

class MetaDataStoreBuilder
{
public:
    MetaDataStoreBuilder(MetaDataStoreInterface& mdstore,
                         backend::BackendInterfacePtr bi,
                         const boost::filesystem::path& scratch_dir);

    ~MetaDataStoreBuilder();

    MetaDataStoreBuilder(const MetaDataStoreBuilder&) = delete;

    MetaDataStoreBuilder&
    operator=(const MetaDataStoreBuilder&) = delete;

    struct Result
    {
        NSIDMap nsid_map;
        size_t num_tlogs = 0;
    };

    // DryRun: don't apply to mdstore
    Result
    operator()(const boost::optional<youtils::UUID>& end_cork = boost::none,
               CheckScrubId check_scrub_id = CheckScrubId::T,
               DryRun dry_run = DryRun::F);

private:
    DECLARE_LOGGER("MetaDataStoreBuilder");

    MetaDataStoreInterface& mdstore_;
    backend::BackendInterfacePtr bi_;
    const boost::filesystem::path scratch_dir_;

    Result
    update_metadata_store_(const boost::optional<youtils::UUID>& from,
                           const boost::optional<youtils::UUID>& to,
                           CheckScrubId check_scrub_id,
                           DryRun dry_run);
};

}

#endif // !VD_META_DATA_STORE_BUILDER_H_
