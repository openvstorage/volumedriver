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

#include "BackendFactory.h"
#include "FileBackendFactory.h"
#include "MemoryBackend.h"
#include "NullBackend.h"

namespace volumedriver
{

namespace failovercache
{

namespace fs = boost::filesystem;
namespace vd = volumedriver;
namespace yt = youtils;

namespace
{

struct FileBackendFactoryInitializer
    : public boost::static_visitor<boost::optional<FileBackendFactory>>
{
    boost::optional<FileBackendFactory>
    operator()(const FileBackend::Config& cfg) const
    {
        return FileBackendFactory(cfg);
    }

    template<typename T>
        boost::optional<FileBackendFactory>
    operator()(const T&) const
    {
        return boost::none;
    }
};

struct BackendCreator
    : public boost::static_visitor<std::unique_ptr<Backend>>
{
    const boost::optional<FileBackendFactory>& file_backend_factory_;
    const std::string& nspace;
    const vd::ClusterSize csize;

    BackendCreator(const boost::optional<FileBackendFactory>& factory,
                   const std::string& ns,
                   const vd::ClusterSize cs)
        : file_backend_factory_(factory)
        , nspace(ns)
        , csize(cs)
    {}

    std::unique_ptr<Backend>
    operator()(const FileBackend::Config&) const
    {
        ASSERT(file_backend_factory_);
        return file_backend_factory_->make_one(nspace,
                                               csize);
    }

    std::unique_ptr<Backend>
    operator()(const MemoryBackend::Config&) const
    {
        ASSERT(not file_backend_factory_);
        return std::make_unique<MemoryBackend>(nspace,
                                               csize);
    }

    std::unique_ptr<Backend>
    operator()(const NullBackend::Config&) const
    {
        ASSERT(not file_backend_factory_);
        return std::make_unique<NullBackend>(nspace,
                                             csize);
    }

};

}

BackendFactory::BackendFactory(const Config& config)
    : factory_config_(config)
    , file_backend_factory_(boost::apply_visitor(FileBackendFactoryInitializer(),
                                                 factory_config_))
{}

std::unique_ptr<Backend>
BackendFactory::make_backend(const std::string& nspace,
                             const vd::ClusterSize csize)
{
    return boost::apply_visitor(BackendCreator(file_backend_factory_,
                                               nspace,
                                               csize),
                                factory_config_);
}

}

}
