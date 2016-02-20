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

#ifndef SCO_FETCHER_H_
#define SCO_FETCHER_H_

#include "ClusterLocation.h"
#include "FailOverCacheProxy.h"
#include "SCO.h"
#include "Types.h"
#include "VolumeInterface.h"

#include <boost/filesystem.hpp>

#include <youtils/CheckSum.h>
#include <youtils/FileDescriptor.h>
#include <youtils/FileUtils.h>
#include <youtils/Logging.h>

namespace volumedriver
{

class FailOverCacheConfig;
class Volume;

class SCOFetcher
{
public:
    virtual ~SCOFetcher() {};

    virtual void
    operator()(const boost::filesystem::path& dest) = 0;

    virtual bool
    disposable() const = 0;
};

class BackendSCOFetcher
    : public SCOFetcher
{
public:
    BackendSCOFetcher(SCO sconame,
                      VolumeInterface* vol,
                      BackendInterfacePtr bi,
                      bool signal_error = true);

    BackendSCOFetcher(const BackendSCOFetcher&) = delete;
    BackendSCOFetcher& operator=(const BackendSCOFetcher&) = delete;

    virtual ~BackendSCOFetcher() = default;

    virtual void
    operator()(const boost::filesystem::path& dest) override final;

    virtual bool
    disposable() const override final;

private:
    DECLARE_LOGGER("BackendSCOFetcher");

    SCO sconame_;
    VolumeInterface* vol_;
    BackendInterfacePtr bi_;
    bool signal_error_;
};

class FailOverCacheSCOFetcher
    : public SCOFetcher
{
public:
    FailOverCacheSCOFetcher(SCO sconame,
                            const CheckSum* checksum,
                            VolumeInterface* vol);

    FailOverCacheSCOFetcher(const FailOverCacheSCOFetcher&) = delete;

    FailOverCacheSCOFetcher&
    operator=(const FailOverCacheSCOFetcher&) = delete;

    virtual void
    operator()(const boost::filesystem::path& dest) override final;

    virtual bool
    disposable() const override final;

private:
    DECLARE_LOGGER("FailOverCacheSCOFetcher");

    SCO sconame_;
    const CheckSum* expected_;
    CheckSum calc_;
    VolumeInterface* vol_;
    std::unique_ptr<youtils::FileDescriptor> sio_;
};

class RawFailOverCacheSCOFetcher
    : public SCOFetcher
{
public:
    RawFailOverCacheSCOFetcher(SCO,
                               const FailOverCacheConfig&,
                               const Namespace&,
                               const LBASize,
                               const ClusterMultiplier,
                               unsigned timeout = 30);

    RawFailOverCacheSCOFetcher(const RawFailOverCacheSCOFetcher&) = delete;

    RawFailOverCacheSCOFetcher
    operator=(const RawFailOverCacheSCOFetcher&) = delete;

    virtual void
    operator()(const boost::filesystem::path& dest) override final;

    virtual bool
    disposable() const override final;

private:
    DECLARE_LOGGER("RawFailOverCacheSCOFetcher");

    SCO sconame_;
    CheckSum calc_;
    std::unique_ptr<FailOverCacheProxy> foc;
    std::unique_ptr<youtils::FileDescriptor> sio_;
};

}

#endif // !SCO_FETCHER_H_

// Local Variables: **
// mode: c++ **
// End: **
