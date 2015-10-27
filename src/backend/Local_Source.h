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
