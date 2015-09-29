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
