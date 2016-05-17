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

#ifndef VD_TLOG_ID_H_
#define VD_TLOG_ID_H_

#include <iosfwd>
#include <vector>

#include <boost/serialization/strong_typedef.hpp>

#include <youtils/UUID.h>

// We're not using youtils/OurStrongTypedef.h as we want our own streaming.
namespace volumedriver
{

BOOST_STRONG_TYPEDEF(youtils::UUID,
                     TLogId);

template<class Archive>
void serialize(Archive& ar,
               TLogId& tlog_id,
               const unsigned)
{
    ar & static_cast<youtils::UUID&>(tlog_id);
}

std::ostream&
operator<<(std::ostream&,
           const TLogId&);

std::istream&
operator>>(std::istream&,
           TLogId&);

using OrderedTLogIds = std::vector<TLogId>;

}

#endif // !VD_TLOG_ID_H_
