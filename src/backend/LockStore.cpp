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

#include "LockStore.h"

#include <youtils/FileUtils.h>
#include <youtils/Logging.h>

namespace backend
{

namespace fs = boost::filesystem;
namespace yt = youtils;

namespace
{

const std::string&
lock_name()
{
    static const std::string s("global_advisory_namespace_lock");
    return s;
}

}

LockStore::LockStore(BackendInterfacePtr bi)
    : bi_(std::move(bi))
{
    if (not bi_->unique_tag_support())
    {
        LOG_ERROR("Backend type cannot be used as LockStore as it doesn't support the extended API");
        throw BackendNotImplementedException();
    }
}

bool
LockStore::exists()
{
    return bi_->objectExists(lock_name());
}

std::tuple<yt::HeartBeatLock, std::unique_ptr<yt::UniqueObjectTag>>
LockStore::read()
{
    const fs::path p(yt::FileUtils::create_temp_file_in_temp_dir(bi_->getNS().str() + "." + lock_name()));
    ALWAYS_CLEANUP_FILE(p);

    std::unique_ptr<yt::UniqueObjectTag> tag(bi_->read_tag(p,
                                                           lock_name()));

    yt::FileDescriptor fd(p,
                          yt::FDMode::Read);
    std::vector<char> vec(fd.size());
    size_t r = fd.read(vec.data(),
                       vec.size());

    VERIFY(r == vec.size());

    return std::make_tuple(yt::HeartBeatLock(std::string(vec.data(),
                                                         vec.size())),
                           std::move(tag));
}

std::unique_ptr<yt::UniqueObjectTag>
LockStore::write(const yt::HeartBeatLock& lock,
                 const yt::UniqueObjectTag* prev_tag)
{
    std::string s;
    lock.save(s);

    const fs::path p(yt::FileUtils::create_temp_file_in_temp_dir(bi_->getNS().str() + "." + lock_name()));
    ALWAYS_CLEANUP_FILE(p);

    yt::FileDescriptor fd(p,
                          yt::FDMode::Write);
    size_t r = fd.write(s.data(),
                        s.size());

    VERIFY(r == s.size());

    return bi_->write_tag(p,
                          lock_name(),
                          prev_tag,
                          prev_tag ?
                          OverwriteObject::T :
                          OverwriteObject::F);
}

const std::string&
LockStore::name() const
{
    return bi_->getNS().str();
}

void
LockStore::erase()
{
    return bi_->remove(lock_name(),
                       ObjectMayNotExist::T);
}

}
