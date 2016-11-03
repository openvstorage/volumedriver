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

#include "AlbaSequence.h"
#include "Condition.h"

#include <alba/checksum.h>
#include <alba/proxy_client.h>
#include <alba/proxy_protocol.h>
#include <alba/proxy_sequences.h>

#include <youtils/ObjectDigest.h>

namespace backend
{

namespace albaconn
{

namespace apc = ::alba::proxy_client;
namespace app = ::alba::proxy_protocol;
namespace aps = ::alba::proxy_client::sequences;
namespace fs = boost::filesystem;
namespace yt = youtils;

Sequence::Sequence(size_t updates_hint,
                   size_t asserts_hint)
{
    if (asserts_hint)
    {
        asserts_.reserve(asserts_hint);
    }

    if (updates_hint)
    {
        updates_.reserve(updates_hint);
    }
}

Sequence&
Sequence::add_assert(const std::string& object_name,
                     const bool exist)
{
    if (exist)
    {
        asserts_.emplace_back(std::make_shared<aps::AssertObjectExists>(object_name));
    }
    else
    {
        asserts_.emplace_back(std::make_shared<aps::AssertObjectDoesNotExist>(object_name));
    }

    return *this;
}

Sequence&
Sequence::add_assert(const Condition& cond)
{
    const auto& osha = dynamic_cast<const yt::ObjectSha1&>(cond.object_tag());

    // TODO: alba::Sha1 should accept a const string& or move from a string instead of expecting a string&
    std::string s(reinterpret_cast<const char*>(osha.digest().bytes()),
                  osha.digest().size());
    asserts_.emplace_back(std::make_shared<aps::AssertObjectHasChecksum>(cond.object_name(),
                                                                         std::make_unique<::alba::Sha1>(s)));

    return *this;
}

Sequence&
Sequence::add_write(const std::string& object_name,
                    const fs::path& src,
                    const ::alba::Checksum* chksum)
{
    updates_.emplace_back(std::make_shared<aps::UpdateUploadObjectFromFile>(object_name,
                                                                            src.string(),
                                                                            chksum));
    return *this;
}

Sequence&
Sequence::add_delete(const std::string& object_name)
{
    updates_.emplace_back(std::make_shared<aps::UpdateDeleteObject>(object_name));
    return *this;
}

void
Sequence::apply(apc::Proxy_client& client,
                const Namespace& nspace) const
{
    client.apply_sequence(nspace.str(),
                          apc::write_barrier::F,
                          asserts_,
                          updates_);
}

}

}
