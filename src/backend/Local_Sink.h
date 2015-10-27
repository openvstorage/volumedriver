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
