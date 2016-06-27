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

#include "Backend.h"

#include <boost/filesystem.hpp>

#include <youtils/FileDescriptor.h>
#include <youtils/Logging.h>

namespace distributed_transaction_log
{

class BackendFactory
{
public:
    explicit BackendFactory(const boost::optional<boost::filesystem::path>&);

    ~BackendFactory();

    BackendFactory(const BackendFactory&) = delete;

    BackendFactory&
    operator=(const BackendFactory&) = delete;

    std::unique_ptr<Backend>
    make_backend(const std::string&,
                 const volumedriver::ClusterSize);

private:
    DECLARE_LOGGER("DtlBackendFactory");

    std::unique_ptr<youtils::FileDescriptor> lock_fd_;
    boost::unique_lock<youtils::FileDescriptor> file_lock_;

    const boost::optional<boost::filesystem::path> root_;
};

}
