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

#include <boost/iostreams/stream.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/timer.hpp>

#include "backend/BackendConfig.h"
#include "backend/BackendInterface.h"
#include "backend/BackendException.h"
#include "../ManagedBackendSink.h"
#include "../ManagedBackendSource.h"
#include <youtils/FileUtils.h>
#include <youtils/DimensionedValue.h>

#include "BackendTestBase.h"

namespace backend
{
using namespace youtils;

class BackendPerfTest
    : public BackendTestBase
{
public:
    BackendPerfTest()
        : BackendTestBase("BackendPerfTest")
    {}

    virtual void
    SetUp() final override
    {
        BackendTestBase::SetUp();
        nspace_ = make_random_namespace();

    }

    virtual void
    TearDown() final override
    {
        nspace_.reset();
        BackendTestBase::TearDown();

    }

protected:
    std::unique_ptr<WithRandomNamespace> nspace_;

    DECLARE_LOGGER("BackendPerfTest");
};

TEST_F(BackendPerfTest, raw)
{
    const std::string objname("test");
    size_t size = 1 << 28;
    size_t bufsize = 32 << 20;

    {
        const std::vector<char> wbuf(bufsize);

        try
        {
            ManagedBackendSink sink(cm_,
                                    nspace_->ns(),
                                    objname);

            boost::timer t;

            for (size_t i = 0; i < size; i += wbuf.size())
            {
                size_t s = std::min(wbuf.size(), size - i);
                sink.write(&wbuf[0], s);
            }

            sink.close();

            LOG_INFO("Writing " << DimensionedValue(size).toString() << " took " <<
                     t.elapsed() << " seconds");
        }
        catch(BackendNotImplementedException&)
        {
            SUCCEED() << "Streaming not implemented for this backend, exiting test";
            return;
        }
    }

    {
        std::vector<char> rbuf(bufsize);
        ManagedBackendSource src(cm_,
                                 nspace_->ns(),
                                 objname);

        boost::timer t;

        for (size_t i = 0; i < size; i += rbuf.size())
        {
            size_t s = std::min(rbuf.size(), size - i);
            src.read(&rbuf[0], s);
        }

        src.close();

        LOG_INFO("Reading " << DimensionedValue(size).toString() << " took " <<
                 t.elapsed() << " seconds");

    }
}

TEST_F(BackendPerfTest, iostreams)
{
    const std::string objname("test");
    size_t size = 1 << 28;
    size_t bufsize = 32 << 20;

    {
        boost::timer t;

        const std::vector<char> wbuf(bufsize);
        try
        {
            ManagedBackendSink sink(cm_,
                                    nspace_->ns(),
                                    objname);

            ios::stream<ManagedBackendSink> os(sink, bufsize);

            for (size_t i = 0; i < size; i += wbuf.size())
            {
                size_t s = std::min(wbuf.size(), size - i);
                os.write(&wbuf[0], s);
            }

            os.flush();

            LOG_INFO("Writing " << DimensionedValue(size).toString() << " took " <<
                     t.elapsed() << " seconds");
        }
        catch(BackendNotImplementedException&e)
        {
            SUCCEED() << "Streaming not implemented for this backend, exiting test";
            return;
        }
    }

    {
        boost::timer t;

        std::vector<char> rbuf(bufsize);
        ManagedBackendSource src(cm_,
                                 nspace_->ns(),
                                 objname);
        ios::stream<ManagedBackendSource> is(src, bufsize);

        for (size_t i = 0; i < size; i += rbuf.size())
        {
            size_t s = std::min(rbuf.size(), size - i);
            is.read(&rbuf[0], s);
        }

        LOG_INFO("Reading " << DimensionedValue(size).toString() << " took " <<
                 t.elapsed() << " seconds");

    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
