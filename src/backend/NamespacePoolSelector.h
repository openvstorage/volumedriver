// Copyright (C) 2017 iNuron NV
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

#ifndef BACKEND_NAMESPACE_POOL_SELECTOR_H_
#define BACKEND_NAMESPACE_POOL_SELECTOR_H_

#include <memory>

#include <youtils/Logging.h>

namespace backend
{

class BackendConnectionManager;
class Namespace;
class ConnectionPool;

class NamespacePoolSelector
{
public:
    NamespacePoolSelector(const BackendConnectionManager&,
                          const Namespace&);

    ~NamespacePoolSelector() = default;

    NamespacePoolSelector(const NamespacePoolSelector&) = delete;

    NamespacePoolSelector&
    operator=(const NamespacePoolSelector&) = delete;

    const std::shared_ptr<ConnectionPool>&
    pool();

    void
    connection_error();

private:
    DECLARE_LOGGER("NamespacePoolSelector");

    const BackendConnectionManager& cm_;
    const Namespace& nspace_;
    const size_t idx_;
    size_t last_idx_;
};

}

#endif //!BACKEND_NAMESPACE_POOL_SELECTOR_H_
