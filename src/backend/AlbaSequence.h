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

#ifndef BACKEND_ALBA_SEQUENCE_H_
#define BACKEND_ALBA_SEQUENCE_H_

#include "Condition.h"
#include "Namespace.h"

#include <memory>
#include <vector>

#include <boost/filesystem/path.hpp>

namespace alba
{

class Checksum;

namespace proxy_client
{

class Proxy_client;

namespace sequences
{
class Assert;
class Update;
}

}

}

namespace backend
{

class Condition;

namespace albaconn
{

class Sequence
{
    using Asserts = std::vector<std::shared_ptr<::alba::proxy_client::sequences::Assert>>;
    using Updates = std::vector<std::shared_ptr<::alba::proxy_client::sequences::Update>>;

public:
    explicit Sequence(size_t updates_hint = 0,
                      size_t asserts_hint = 0);

    ~Sequence() = default;

    Sequence(const Sequence&) = delete;

    Sequence(Sequence&&) = default;

    Sequence&
    operator=(const Sequence&) = delete;

    Sequence&
    operator=(Sequence&&) = default;

    Sequence&
    add_assert(const std::string&,
               const bool exists);

    Sequence&
    add_assert(const Condition&);

    Sequence&
    add_write(const std::string&,
              const boost::filesystem::path&,
              const ::alba::Checksum*);

    Sequence&
    add_delete(const std::string&);

    void
    apply(::alba::proxy_client::Proxy_client&,
          const Namespace&) const;

private:
    Asserts asserts_;
    Updates updates_;
};

}

}

#endif // !BACKEND_ALBA_SEQUENCE_H_
