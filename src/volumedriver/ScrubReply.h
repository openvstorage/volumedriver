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

#ifndef SCRUB_REPLY_H_
#define SCRUB_REPLY_H_

#include "Types.h"

#include <iosfwd>
#include <string>

#include <youtils/Serialization.h>
#include <backend/Namespace.h>

namespace scrubbing
{
struct ScrubReply
{
    ScrubReply(const backend::Namespace& ns,
               const volumedriver::SnapshotName& snapshot_name,
               const std::string& scrub_result_name)
        : ns_(ns)
        , snapshot_name_(snapshot_name)
        , scrub_result_name_(scrub_result_name)
    {}

    ScrubReply() = default;

    explicit ScrubReply(const std::string&);

    ~ScrubReply() = default;

    ScrubReply(const ScrubReply&) = default;

    ScrubReply&
    operator=(const ScrubReply&) = default;

    bool
    operator==(const ScrubReply&) const;

    bool
    operator!=(const ScrubReply& other) const;

    bool
    operator<(const ScrubReply&) const;

    std::string
    str() const;

    template<class Archive>
    void
    serialize(Archive& ar,
              const unsigned int version)
    {
        CHECK_VERSION(version, 2);

        ar & BOOST_SERIALIZATION_NVP(ns_);
        ar & BOOST_SERIALIZATION_NVP(snapshot_name_);
        ar & BOOST_SERIALIZATION_NVP(scrub_result_name_);
    }

    backend::Namespace ns_;
    volumedriver::SnapshotName snapshot_name_;
    std::string scrub_result_name_;
};

std::ostream&
operator<<(std::ostream&,
           const ScrubReply&);

}

BOOST_CLASS_VERSION(scrubbing::ScrubReply, 2);

#endif // SCRUB_REPLY_H_
