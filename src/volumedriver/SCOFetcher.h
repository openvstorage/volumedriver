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

#ifndef SCO_FETCHER_H_
#define SCO_FETCHER_H_

#include "ClusterLocation.h"
#include "DtlProxy.h"
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
                               const boost::chrono::seconds timeout =
                               boost::chrono::seconds(30));

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
    std::unique_ptr<DtlProxy> foc;
    std::unique_ptr<youtils::FileDescriptor> sio_;
};

}

#endif // !SCO_FETCHER_H_

// Local Variables: **
// mode: c++ **
// End: **
