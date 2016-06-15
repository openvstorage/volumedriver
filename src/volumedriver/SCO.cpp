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

#include "SCO.h"
#include <youtils/IOException.h>
#include <youtils/Assert.h>
#include <iomanip>

namespace volumedriver
{
static_assert(sizeof(SCO) == 6, "unexpected sco size, not 8");

std::ostream&
operator<<(std::ostream& ostr,
           const volumedriver::SCO& loc)
{
    return ostr << loc.str();
}

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream& ostream,
           const SCO& sconame)
{
    return ostream << sconame.version_ << sconame.number_ << sconame.cloneID_;
}

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream& ostream,
           SCO& sconame)
{
    ostream >> sconame.version_;
    uint32_t number;
    ostream >> number;
    sconame.number_ = number;
    ostream >> sconame.cloneID_;
    return ostream;

}

bool
SCO::hasonlyhexdigits(const std::string& str,
                           size_t i1,
                           size_t i2)
{
    VERIFY(i1 <= i2);
    for(size_t i = i1; i < i2; ++i)
    {
        if(not std::isxdigit(str[i]))
        {
            return false;
        }
    }
    return true;
}

bool
SCO::isSCOString(const std::string& str)
{
    return str.length() == 14 and
        hasonlyhexdigits(str,0,2) and

        str[2] == '_' and
        hasonlyhexdigits(str,3,11) and
        str[11] == '_' and
        hasonlyhexdigits(str,12,14);
}

std::string
SCO::str() const
{
    std::stringstream ss;
    ss << std::hex
       << std::setfill('0') << std::setw(2) << static_cast<int>(cloneID_)
       << "_"
       << std::setw(8) << number_
       << "_"
       << std::setw(2) << static_cast<int>(version_)
       << std::dec;
    return ss.str();
}

uint64_t
SCO::gethexnum(const std::string& str,
               size_t i1,
               size_t i2)
{
    VERIFY(i1 < i2);
    uint64_t res = 0;

    for(size_t i = i1; i < i2; ++i)
    {
        res = res << 4;
        char val = str[i];

        switch(val)
        {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            res += (val - '0');
        break;

        case 'a':
        case 'A':
            res += 0xa;
        break;

        case 'b':
        case 'B':
            res += 0xb;
        break;

        case 'c':
        case 'C':
            res += 0xc;
        break;

        case 'd':
        case 'D':
            res += 0xd;
        break;

        case 'e':
        case 'E':
            res += 0xe;
        break;

        case 'f':
        case 'F':
            res += 0xf;
        break;
        default:
            LOG_ERROR("Not a valid hexadecimal character " << val);
            throw fungi::IOException("Not a valid hexadecimal character ");
        }
    }
    return res;
}

SCO::SCO(const std::string& str)
    : version_(0)
    , number_(0)
    , cloneID_(0)
{
    if(not isSCOString(str))
    {
        LOG_DEBUG("Could make a sconame out of " << str);

        throw fungi::IOException("Not a SCOName string");
    }
    cloneID_ = gethexnum(str,0,2);
    number_ = gethexnum(str,3,11);
    version_ = gethexnum(str, 12,14);

}

bool
SCO::operator==(const SCO& other) const
{
    return version_ == other.version_ and
        number_ == other.number_ and
        cloneID_ == other.cloneID_;
}

bool
SCO::operator!=(const SCO& other) const
{
    return version_ != other.version_ or
        number_ != other.number_ or
        cloneID_ != other.cloneID_;
}


}

// Local Variables: **
// mode: c++ **
// End: **
