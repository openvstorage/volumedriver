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

#include "OneFileTLogReader.h"
#include "TLogWriter.h"
#include "VolumeDriverError.h"

#include <youtils/FileUtils.h>
#include <backend/BackendInterface.h>

namespace volumedriver
{
OneFileTLogReader::OneFileTLogReader(const fs::path& TLogPath,
                                     const std::string& TLogName,
                                     BackendInterfacePtr bi)
    : file_(nullptr)
    , unlinkOnDestruction_(false)
{
    LOG_TRACE(TLogPath << ", " << TLogName);

    fs::path tmp;

    if(fs::exists(TLogPath / TLogName))
    {
        tmp = TLogPath / TLogName;
    }
    else
    {
        // Do the TransparentMagic here!!
        tmp = FileUtils::create_temp_file(TLogPath, TLogName + "_temporary");
        try
        {
            VERIFY(bi);
            bi->read(tmp, TLogName, InsistOnLatestVersion::F);
        }
        CATCH_STD_ALL_EWHAT({
                VolumeDriverError::report(events::VolumeDriverErrorCode::GetTLogFromBackend,
                                          EWHAT,
                                          VolumeId(bi->getNS().str()));
                throw;
            });

        unlinkOnDestruction_ = true;
    }
    setFileSize_(tmp);

    try
    {
        file_.reset(new FileDescriptor(tmp, FDMode::Read));
    }
    CATCH_STD_ALL_EWHAT({
            VolumeDriverError::report(events::VolumeDriverErrorCode::ReadTLog,
                                      EWHAT,
                                      VolumeId(bi->getNS().str()));
            throw;
        });
}

OneFileTLogReader::OneFileTLogReader(const fs::path& path)
    : file_(nullptr)
    , unlinkOnDestruction_(false)
{
    LOG_TRACE(path);
    try
    {
        setFileSize_(path);
        file_.reset(new FileDescriptor(path,FDMode::Read));
    }
    CATCH_STD_ALL_EWHAT({
            VolumeDriverError::report(events::VolumeDriverErrorCode::ReadTLog,
                                      EWHAT);
            throw;
        });
}

OneFileTLogReader::~OneFileTLogReader()
{
    if(file_.get())
    {
        if (unlinkOnDestruction_)
        {
            fs::remove(file_->path());
        }

        try
        {
            //   file_->close();
        }
        CATCH_STD_ALL_LOG_IGNORE("Error closing file");
    }
}

void
OneFileTLogReader::setFileSize_(const fs::path& path)
{
    int64_t filesize = fs::file_size(path);
    int64_t rest = (filesize % Entry::getDataSize());
    if (rest != 0)
    {
        LOG_WARN("trailing garbage in " << path.string()
                 << " ignored (" << rest << " bytes JOE)");
        try
        {
            FileUtils::truncate(path, filesize - rest);
        }
        CATCH_STD_ALL_EWHAT({
                VolumeDriverError::report(events::VolumeDriverErrorCode::WriteTLog,
                                          EWHAT);
                throw;
            });
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
