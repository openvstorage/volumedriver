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
