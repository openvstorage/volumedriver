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

#ifndef BACKEND_SIMPLE_FETCHER_H_
#define BACKEND_SIMPLE_FETCHER_H_

#include "BackendConnectionInterface.h"
#include "Namespace.h"

#include <boost/filesystem.hpp>

#include <youtils/FileDescriptor.h>
#include <youtils/Logging.h>

namespace backend
{

class SimpleFetcher
    : public BackendConnectionInterface::PartialReadFallbackFun
{
public:
    SimpleFetcher(BackendConnectionInterface& conn,
                  const Namespace& nspace,
                  const boost::filesystem::path& home);

    virtual ~SimpleFetcher();

    SimpleFetcher(const SimpleFetcher&) = delete;

    SimpleFetcher&
    operator=(const SimpleFetcher&) = delete;

    virtual youtils::FileDescriptor&
    operator()(const Namespace& ns,
               const std::string& object_name,
               InsistOnLatestVersion) override final;

private:
    DECLARE_LOGGER("SimpleFetcher");

    BackendConnectionInterface& conn_;
    const Namespace nspace_;
    const boost::filesystem::path home_;

    using FDMap = std::map<std::string,
                           std::unique_ptr<youtils::FileDescriptor>>;
    FDMap fd_map_;
};

}

#endif // !BACKEND_SIMPLE_FETCHER_H_
