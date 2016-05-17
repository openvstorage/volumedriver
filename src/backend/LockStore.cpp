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

#include <youtils/Logging.h>

namespace backend
{

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
    if (not bi_->hasExtendedApi())
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

std::tuple<yt::HeartBeatLock, yt::GlobalLockTag>
LockStore::read()
{
    std::string s;
    const ETag etag(bi_->x_read(s,
                                lock_name(),
                                InsistOnLatestVersion::T).etag_);
    return std::make_tuple(yt::HeartBeatLock(s),
                           static_cast<const yt::GlobalLockTag>(etag));
}

yt::GlobalLockTag
LockStore::write(const yt::HeartBeatLock& lock,
                 const boost::optional<yt::GlobalLockTag>& lock_tag)
{
    std::string s;
    lock.save(s);

    const ETag etag(bi_->x_write(s,
                                 lock_name(),
                                 lock_tag ?
                                 OverwriteObject::T :
                                 OverwriteObject::F,
                                 lock_tag ?
                                 &reinterpret_cast<const ETag&>(*lock_tag) :
                                 nullptr).etag_);
    return reinterpret_cast<const yt::GlobalLockTag&>(etag);
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
