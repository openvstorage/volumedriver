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

#include "ClusterLocation.h"
#include "DataStoreNG.h"
#include "FailOverCacheConfig.h"
#include "SCOFetcher.h"
#include "Volume.h"
#include "VolumeDriverError.h"

#include <youtils/FileDescriptor.h>
#include <youtils/FileUtils.h>
#include <youtils/IOException.h>

#include <backend/BackendInterface.h>
#include <backend/BackendException.h>

namespace volumedriver
{

namespace fs = boost::filesystem;

BackendSCOFetcher::BackendSCOFetcher(SCO sconame,
                                     VolumeInterface* vol,
                                     BackendInterfacePtr bi,
                                     bool signal_error)
    : sconame_(sconame)
    , vol_(vol)
    , bi_(std::move(bi))
    , signal_error_(signal_error)
{
    sconame_.cloneID(SCOCloneID(0));
}

void
BackendSCOFetcher::operator()(const fs::path& dst)
{
    try
    {
        bi_->read(dst,
                  sconame_.str(),
                  InsistOnLatestVersion::F);
    }
    catch (backend::BackendOutputException& e)
    {
        LOG_ERROR("Failed to fetch " << dst << ": " << e.what());
        VERIFY(vol_);
        if( signal_error_)
        {
            VolumeDriverError::report(events::VolumeDriverErrorCode::GetSCOFromBackend,
                                      e.what(),
                                      vol_->getName());
        }
        throw SCOCacheMountPointIOException();
    }
    CATCH_STD_ALL_EWHAT({
            VERIFY(vol_);

            if(signal_error_)
            {
                VolumeDriverError::report(events::VolumeDriverErrorCode::GetSCOFromBackend,
                                          EWHAT,
                                          vol_->getName());
            }

            throw;
        });
}

bool
BackendSCOFetcher::disposable() const
{
    return true;
}

FailOverCacheSCOFetcher::FailOverCacheSCOFetcher(SCO sconame,
                                                 const CheckSum* expected,
                                                 VolumeInterface* vol)
    : sconame_(sconame)
    , expected_(expected)
    , calc_()
    , vol_(vol)
    , sio_(nullptr)
{
    VERIFY(sconame.cloneID() == 0);
}

void
FailOverCacheSCOFetcher::operator()(const fs::path& dst)
{
    fs::path tmp;
    try
    {
        tmp = FileUtils::create_temp_file(dst);
    }
    catch (fungi::IOException& e)
    {
        LOG_ERROR("Failed to create temp file for " << dst << ": " << e.what());
        throw SCOCacheMountPointIOException();
    }

    ALWAYS_CLEANUP_FILE(tmp);

    try
    {
        sio_.reset(new FileDescriptor(tmp,
                                      FDMode::Write));


        // this needs to be reworked once ->getSCO throws if it cannot find
        // the SCO.

        auto fun([&](ClusterLocation loc,
                     uint64_t /* lba */,
                     const byte* buf,
                     size_t size)
                 {
                     VERIFY(sio_.get());
                     //    VERIFY(sconame_ != 0);

                     VERIFY(loc.sco() == sconame_);

                     sio_->write(buf,
                                 size);

                     calc_.update(buf, size);
                 });

        uint64_t scosize = vol_->getFailOver()->getSCOFromFailOver(sconame_,
                                                                   std::move(fun));
        if (scosize == 0)
        {
            std::stringstream ss;
            ss << static_cast<std::string>(vol_->getName()) <<
                ": failed to get SCO " << sconame_;

            LOG_ERROR(ss.str());
            vol_->halt();
            throw fungi::IOException(ss.str().c_str());
        }

        sio_->sync();

        if (expected_ and
            calc_ != *expected_)
        {
            LOG_FATAL(vol_->getName() << ": checksum mismatch: got " << calc_ <<
                      ", expected " << expected_);
            vol_->halt();
            throw CheckSumException();
        }

        fs::rename(tmp, dst);
    }
    catch (youtils::FileDescriptorException& e)
    {
        LOG_ERROR("IO Error: " << e.what());
        VERIFY(vol_);
        VolumeDriverError::report(events::VolumeDriverErrorCode::GetSCOFromFOC,
                                  e.what(),
                                  vol_->getName());
        throw SCOCacheMountPointIOException();
    }
    catch (FailOverCacheNotConfiguredException& e)
    {
        VERIFY(vol_);
        LOG_ERROR(vol_->getName() << ": cannot get SCO " <<
                  sconame_ << " from FailOverCache: " <<
                  e.what());
        vol_->halt();
        VolumeDriverError::report(events::VolumeDriverErrorCode::GetSCOFromFOC,
                                  e.what(),
                                  vol_->getName());
        throw;
    }
    CATCH_STD_ALL_EWHAT({
            VERIFY(vol_);
            LOG_ERROR(vol_->getName() << ": cannot get SCO " <<
                      sconame_ << " from FailOverCache: " <<
                      EWHAT);

            VolumeDriverError::report(events::VolumeDriverErrorCode::GetSCOFromFOC,
                                      EWHAT,
                                      vol_->getName());
            throw;
        });
}

bool
FailOverCacheSCOFetcher::disposable() const
{
    return false;
}

RawFailOverCacheSCOFetcher::RawFailOverCacheSCOFetcher(SCO sconame,
                                                       const FailOverCacheConfig& cfg,
                                                       const Namespace& ns,
                                                       const LBASize lba_size,
                                                       const ClusterMultiplier cmult,
                                                       unsigned timeout)
    : sconame_(sconame)
{
    VERIFY(sconame.cloneID() == 0);
    try
    {
        foc = std::make_unique<FailOverCacheProxy>(cfg,
                                                   ns,
                                                   lba_size,
                                                   cmult,
                                                   timeout);
    }
    CATCH_STD_ALL_LOG_IGNORE("Failed to instantiate FailOverCacheProxy");
}

void
RawFailOverCacheSCOFetcher::operator()(const fs::path& dst)
{
    if(foc)
    {
        fs::path tmp(FileUtils::create_temp_file(dst));
        sio_.reset(new FileDescriptor(tmp,
                                      FDMode::Write));

        ALWAYS_CLEANUP_FILE(tmp);

        // this needs to be reworked once ->getSCO throws if it cannot find
        // the SCO.
        auto fun([&](ClusterLocation loc,
                     uint64_t /* lba */,
                     const byte* buf,
                     size_t size)
                 {
                     VERIFY(sio_.get());

                     VERIFY(loc.sco() == sconame_);

                     sio_->write(buf,
                                 size);
                 });

        uint64_t scosize = foc->getSCOFromFailOver(sconame_,
                                                   std::move(fun));
        if (scosize == 0)
        {
            throw fungi::IOException("Could not get SCO from the foc");
        }

        sio_->sync();
        fs::rename(tmp, dst);
    }
    else
    {
        throw fungi::IOException("FOC not available");
    }

}

bool
RawFailOverCacheSCOFetcher::disposable() const
{
    return false;
}

}

// Local Variables: **
// mode: c++ **
// End: **
