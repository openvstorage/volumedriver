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

#ifndef VOLUME_INTERFACE_H
#define VOLUME_INTERFACE_H

#include "PerformanceCounters.h"
#include "SCO.h"
#include "TLogId.h"
#include "Types.h"
#include "VolumeFailOverState.h"

#include <string>

#include <boost/chrono.hpp>
#include <boost/shared_ptr.hpp>

#include <youtils/BooleanEnum.h>

#include <backend/BackendInterface.h>
#include <backend/Condition.h>

namespace volumedriver
{

class Volume;
class VolumeConfig;
class FailOverCacheClientInterface;
class DataStoreNG;
class MetaDataBackendConfig;

VD_BOOLEAN_ENUM(AppendCheckSum);

class VolumeInterface
{
public:
    virtual ~VolumeInterface()
    {};

    virtual const backend::BackendInterfacePtr&
    getBackendInterface(const SCOCloneID id = SCOCloneID(0)) const = 0;

    virtual VolumeInterface*
    getVolumeInterface() = 0;

    // virtual VolumeFailOverState
    // setVolumeFailOverState(VolumeFailOverState) = 0;

    virtual const VolumeId
    getName() const = 0;

    virtual backend::Namespace
    getNamespace() const = 0;

    virtual VolumeConfig
    get_config() const = 0;

    virtual void
    halt() = 0;

    virtual void
    SCOWrittenToBackendCallback(uint64_t file_size,
                                boost::chrono::microseconds write_time) = 0 ;

    virtual void
    tlogWrittenToBackendCallback(const TLogId&,
                                 const SCO) = 0;

    virtual ClusterSize
    getClusterSize() const = 0;


    virtual SCOMultiplier
    getSCOMultiplier() const = 0;

    virtual boost::optional<TLogMultiplier>
    getTLogMultiplier() const = 0;

    virtual boost::optional<SCOCacheNonDisposableFactor>
    getSCOCacheMaxNonDisposableFactor() const = 0;

    virtual fs::path
    saveSnapshotToTempFile() = 0;

    virtual DataStoreNG*
    getDataStore() = 0;

    virtual FailOverCacheClientInterface*
    getFailOver() = 0;

    // virtual VolumeFailOverState
    // getVolumeFailOverState() const = 0;

    virtual void
    removeUpToFromFailOverCache(const SCO sconame) = 0;

    virtual void
    checkState(const TLogId&) = 0;

    virtual void
    cork(const youtils::UUID&) = 0;

    virtual void
    unCorkAndTrySync(const youtils::UUID&) = 0;

    virtual void
    metaDataBackendConfigHasChanged(const MetaDataBackendConfig& cfg) = 0;

    const boost::shared_ptr<backend::Condition>&
    backend_write_condition() const
    {
        return backend_write_cond_;
    }

    PerformanceCounters&
    performance_counters()
    {
        return perf_counters_;
    }

    const PerformanceCounters&
    performance_counters() const
    {
        return perf_counters_;
    }

    static const std::string&
    owner_tag_backend_name()
    {
        static const std::string n("owner_tag");
        return n;
    }

protected:
    explicit VolumeInterface(const boost::shared_ptr<backend::Condition>& cond)
        : backend_write_cond_(cond)
    {}

private:
    PerformanceCounters perf_counters_;
    boost::shared_ptr<backend::Condition> backend_write_cond_;
};

}

#endif // VOLUME_INTERFACE_H

// Local Variables: **
// mode: c++ **
// End: **
