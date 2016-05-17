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
