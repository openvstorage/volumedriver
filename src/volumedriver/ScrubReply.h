// Copyright 2015 iNuron NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SCRUB_REPLY_H_
#define SCRUB_REPLY_H_

#include "Types.h"

#include <string>
#include <sstream>

#include <youtils/Serialization.h>

#include <backend/Namespace.h>

namespace scrubbing
{
struct ScrubReply
{
    ScrubReply(const backend::Namespace& ns,
               const std::string& scrub_result_name)
        : ns_(ns)
        , scrub_result_name_(scrub_result_name)
    {}

    ScrubReply() = default;

    explicit ScrubReply(const std::string&);

    ~ScrubReply() = default;

    ScrubReply(const ScrubReply&) = default;

    ScrubReply&
    operator=(const ScrubReply&) = default;

    std::string
    str() const;

    template<class Archive>
    void
    serialize(Archive& ar,
              const unsigned int /* version */)
    {
        ar & BOOST_SERIALIZATION_NVP(ns_);

        volumedriver::VolumeId id_;
        ar & BOOST_SERIALIZATION_NVP(id_);

        ar & BOOST_SERIALIZATION_NVP(scrub_result_name_);
    }

    backend::Namespace ns_;
    std::string scrub_result_name_;
};

}

BOOST_CLASS_VERSION(scrubbing::ScrubReply, 1);

#endif // SCRUB_REPLY_H_
