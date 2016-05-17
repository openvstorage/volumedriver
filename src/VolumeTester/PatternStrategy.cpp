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

#include "PatternStrategy.h"

const std::string
PatternStrategy::default_pat_("darling!");

PatternStrategy::PatternStrategy(const std::string& pat)
    : pat_(pat.empty() ? default_pat_ : pat)
    , sz_(pat_.size())
{}

void
PatternStrategy::operator()(unsigned char* buf1,
                            unsigned long blocksize) const
{
  for(size_t i = 0; i < blocksize; i++)
  {
    buf1[i] = pat_[i%sz_];
  }
}

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
