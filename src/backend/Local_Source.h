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

#ifndef LOCAL_SOURCE_H_
#define LOCAL_SOURCE_H_

#include <boost/filesystem.hpp>

#include <youtils/Logging.h>

#include "BackendSourceInterface.h"
#include <youtils/FileDescriptor.h>

namespace backend
{

namespace local
{
using youtils::FileDescriptor;
namespace fs = boost::filesystem;

class Connection;

class Source
    : public BackendSourceInterface
{

public:
    Source(std::unique_ptr<Connection> conn,
           const Namespace& nspace,
           const std::string& name,
           boost::posix_time::time_duration timeout_ms);

    virtual ~Source();

    Source(const Source&) = delete;

    Source&
    operator=(const Source&) = delete;

    virtual std::streamsize
    read(char* s,
         std::streamsize n);

    virtual std::streampos
    seek(ios::stream_offset off,
         std::ios_base::seekdir way);

    virtual void
    close();

private:
    DECLARE_LOGGER("Source");

    std::unique_ptr<Connection> conn_;
    std::unique_ptr<FileDescriptor> sio_;
    boost::posix_time::time_duration timeout_;
};

}
}
#endif // !LOCAL_SOURCE_H_

// Local Variables: **
// mode: c++ **
// End: **
