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

#ifndef VD_META_DATA_STORE_BUILDER_H_
#define VD_META_DATA_STORE_BUILDER_H_

#include "MetaDataStoreInterface.h"
#include "NSIDMap.h"

#include <youtils/Logging.h>

#include <boost/filesystem.hpp>

#include <backend/BackendInterface.h>

namespace volumedriver
{

VD_BOOLEAN_ENUM(CheckScrubId);

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
        bool full_rebuild = false;
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
                           DryRun dry_run,
                           bool full_rebuild);
};

}

#endif // !VD_META_DATA_STORE_BUILDER_H_
