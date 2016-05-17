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

#ifndef LOCAL_SINK_H_
#define LOCAL_SINK_H_

#include <boost/filesystem.hpp>

#include <youtils/Logging.h>

#include "BackendSinkInterface.h"
#include <youtils/FileDescriptor.h>

namespace backend
{

namespace local
{

namespace fs = boost::filesystem;
using youtils::FileDescriptor;

class Connection;

class Sink
    : public BackendSinkInterface
{

public:
    Sink(std::unique_ptr<Connection> conn,
         const Namespace& nspace,
         const std::string& name,
         boost::posix_time::time_duration timeout);

    virtual ~Sink();

    Sink(const Sink&) = delete;

    Sink&
    operator=(const Sink&) = delete;

    virtual std::streamsize
    write(const char* s,
          std::streamsize n);

    virtual void
    close();

private:
    DECLARE_LOGGER("Sink");

    std::unique_ptr<Connection> conn_;
    const fs::path path_;
    std::unique_ptr<FileDescriptor> sio_;
    const boost::posix_time::time_duration timeout_;
};

}
}
#endif // !LOCAL_SINK_H_

// Local Variables: **
// mode: c++ **
// End: **
