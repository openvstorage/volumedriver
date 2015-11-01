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

#include "Local_Connection.h"
#include "Local_Sink.h"

#include <youtils/FileUtils.h>

namespace backend
{

namespace local
{

Sink::Sink(std::unique_ptr<Connection> conn,
           const Namespace& nspace,
           const std::string& name,
           boost::posix_time::time_duration timeout)
    : conn_(std::move(conn))
    , path_(conn_->path() / nspace.str() / name)
    , sio_(nullptr)
    , timeout_(timeout)
{
    try
    {
        sio_.reset(new FileDescriptor(youtils::FileUtils::create_temp_file(path_),
                                      youtils::FDMode::Write,
                                      CreateIfNecessary::T));
    }
    catch (std::exception& e)
    {
        LOG_ERROR("Failed to create temporary output file for " << path_ << ": " <<
                  e.what());
        throw BackendStoreException();
    }
    catch (...)
    {
        LOG_ERROR("Failed to create temporary output file for " << path_ <<
                  ": unknown exception");
        throw BackendStoreException();
    }
}

Sink::~Sink()
{
    if (sio_.get() != 0)
    {
        LOG_ERROR(path_ <<
                  ": stream not closed - object will not be finished and underlying connection will not be reused");
        fs::path sio_path = sio_->path();
        sio_.reset();
        try
        {
            fs::remove_all(sio_path);
        }
        catch (std::exception& e)
        {
            LOG_ERROR(path_ << ": failed to close stream: " << e.what());
        }
        catch (...)
        {
            LOG_ERROR(path_ << ": failed to close stream: unknown exception");
        }
    }
}

std::streamsize
Sink::write(const char* s,
            std::streamsize n)
{
    VERIFY(sio_.get() != 0);
    try
    {
        return sio_->write(s, n);
    }
    catch (std::exception& e)
    {
        LOG_ERROR("Failed to write " << n << " bytes to " << sio_->path() <<
                  ": " << e.what());
        throw BackendStoreException();
    }
    catch (...)
    {
        LOG_ERROR("Failed to write " << n << " bytes to " << sio_->path() <<
                  ": unknown exception");
        throw BackendStoreException();
    }
}

void
Sink::close()
{
    LOG_INFO(path_ << ": closing");
    VERIFY(sio_.get() != 0);
    fs::path sio_path = sio_->path();
    sio_.reset();
    try
    {
        //  sio_->sync();
        fs::rename(sio_path, path_);
    }
    catch (std::exception& e)
    {
        LOG_ERROR(path_ << ": failed to close " << sio_->path() << ": " <<
                  e.what());
        throw BackendStoreException();
    }
    catch (...)
    {
        LOG_ERROR(path_ << ": failed to close "  << sio_->path() <<
                  ": unknown exception");
        throw BackendStoreException();
    }


    conn_.reset();
}

}
}

// Local
// mode: c++ **
// End: **
