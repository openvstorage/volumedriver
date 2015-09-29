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

#ifndef ONEFILETLOGREADER_H
#define ONEFILETLOGREADER_H

#include <boost/utility.hpp>

#include "TLogReaderInterface.h"
#include <youtils/FileDescriptor.h>
#include <youtils/FileUtils.h>

namespace volumedriver
{
using youtils::Whence;
using youtils::FileDescriptor;
using youtils::FDMode;

class OneFileTLogReader
    : public TLogReaderInterface
{
protected:
    OneFileTLogReader(const fs::path& TLogPath,
                      const std::string& tlogName,
                      BackendInterfacePtr bi = 0);

    OneFileTLogReader(const fs::path& path);

    virtual ~OneFileTLogReader();

    std::unique_ptr<FileDescriptor> file_;

    DECLARE_LOGGER("OneFileTLogReader");

private:
    bool unlinkOnDestruction_;

    void
    setFileSize_(const fs::path& path);
};
}

#endif // ONEFILETLOGREADER_H

// Local Variables: **
// mode: c++ **
// End: **
