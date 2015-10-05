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

#ifndef VOLUME_INTERFACE_H
#define VOLUME_INTERFACE_H

#include "PerformanceCounters.h"
#include "SCO.h"
#include "Types.h"
#include "VolumeFailOverState.h"

#include <string>

#include <boost/chrono.hpp>

#include <youtils/BooleanEnum.h>

#include <backend/BackendInterface.h>

namespace volumedriver
{

class Volume;
class VolumeConfig;
class FailOverCacheBridge;
class DataStoreNG;
class MetaDataBackendConfig;

BOOLEAN_ENUM(AppendCheckSum);

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
    tlogWrittenToBackendCallback(const TLogID&,
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

    virtual FailOverCacheBridge*
    getFailOver() = 0;

    // virtual VolumeFailOverState
    // getVolumeFailOverState() const = 0;

    virtual void
    removeUpToFromFailOverCache(const SCO sconame) = 0;

    virtual void
    checkState(const std::string& tlogname) = 0;

    virtual void
    cork(const youtils::UUID&) = 0;

    virtual void
    unCorkAndTrySync(const youtils::UUID&) = 0;

    virtual void
    metaDataBackendConfigHasChanged(const MetaDataBackendConfig& cfg) = 0;

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

private:
    PerformanceCounters perf_counters_;
};

}

#endif // VOLUME_INTERFACE_H

// Local Variables: **
// mode: c++ **
// End: **
