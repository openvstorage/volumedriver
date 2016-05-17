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

#ifndef PATTERN_STRATEGY_H_
#define PATTERN_STRATEGY_H_
#include "Strategy.h"
#include <string>

class PatternStrategy : public Strategy
{
public:
    PatternStrategy(const std::string& pat = "");

    virtual void
    operator()(unsigned char* buf1,
               unsigned long blocksize) const;
private:
    static const std::string default_pat_;
    const std::string pat_;
    const size_t sz_;
};
#endif // PATTERN_STRATEGY_H_
