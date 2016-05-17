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
#include "Local_Source.h"

#include <iostream>

#include <youtils/FileUtils.h>

namespace backend
{

namespace local
{

// Z42: do we really want to throw Backend*Exceptions here?
Source::Source(std::unique_ptr<Connection> conn,
               const Namespace& nspace,
               const std::string& name,
               boost::posix_time::time_duration timeout)
    : conn_(std::move(conn))
    , timeout_(timeout)
{
    const fs::path p(conn_->path() / nspace.str() / name);
    if (not fs::exists(p))
    {
        LOG_ERROR("Object " << p << " does not exist");
        throw BackendObjectDoesNotExistException();
    }

    try
    {
        sio_.reset(new FileDescriptor(p,
                                      youtils::FDMode::Read));
    }
    catch (std::exception& e)
    {
        LOG_ERROR("Failed to open " << p << ": " << e.what());
        throw BackendRestoreException();
    }
    catch (...)
    {
        LOG_ERROR("Failed to open " << p << ": unknown exception");
        throw BackendRestoreException();
    }
}

Source::~Source()
{
    if (sio_.get() != 0)
    {
        LOG_ERROR("Stream not closed - closing connection");
        try
        {
            close();
        }
        catch (std::exception& e)
        {
            LOG_ERROR("Failed to close stream: " << e.what());
        }
        catch (...)
        {
            LOG_ERROR("Failed to close stream: unknown exception");
        }
    }
}

std::streamsize
Source::read(char* s,
             std::streamsize n)
{
    VERIFY(sio_.get() != 0);

    try
    {
        return sio_->read(s, n);
    }
    catch (std::exception& e)
    {
        LOG_ERROR("Failed to read " << n << " bytes from " << sio_->path() <<
                  ": " << e.what());
        throw BackendRestoreException();
    }
    catch (...)
    {
        LOG_ERROR("Failed to read " << n << " bytes from " << sio_->path() <<
                  ": unknown exception");
        throw BackendRestoreException();
    }
}

std::streampos
Source::seek(ios::stream_offset off,
             std::ios_base::seekdir way)
{
    using youtils::Whence;
    Whence w;
    VERIFY(sio_.get() != 0);
    switch (way)
    {
    case std::ios_base::beg:
        w = Whence::SeekSet;
        break;
    case std::ios_base::cur:
        w = Whence::SeekCur;
        break;
    case std::ios_base::end:
        w = Whence::SeekEnd;
        break;
    default:
        VERIFY(0 == "this should not happen");
        break;
    }

    try
    {
        return sio_->seek(off, w);
    }
    catch (std::exception& e)
    {
        LOG_ERROR("Failed to seek " << sio_->path() << " to " << off << ", " <<
                  (unsigned)w << ": " << e.what());
        throw BackendRestoreException();
    }
    catch (...)
    {
        LOG_ERROR("Failed to seek " << sio_->path() << " to " << off << ", " <<
                  (unsigned)w << ": unknown exception");
        throw BackendRestoreException();
    }
}

void
Source::close()
{
    VERIFY(sio_.get() != 0);
    try
    {
        // sio_->close();
    }
    catch (std::exception& e)
    {
        LOG_ERROR("Failed to close " << sio_->path() << ": " << e.what());
        throw BackendRestoreException();
    }
    catch (...)
    {
        LOG_ERROR("Failed to close " << sio_->path() << ": unknown exception");
        throw BackendRestoreException();
    }

    sio_.reset();
    conn_.reset();
}

}
}

// Local Variables: **
// mode: c++ **
// End: **
