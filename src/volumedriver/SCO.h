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

#ifndef SCONAME_H_
#define SCONAME_H_
#include "Types.h"
#include <string>
#include <youtils/Logging.h>
#include "youtils/Serialization.h"
#include "distributed-transaction-log/fungilib/IOBaseStream.h"
#include "Types.h"

namespace volumedriver
{

class SCO;

std::ostream&
operator<<(std::ostream& ostr,
           const SCO& loc);

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream& ostream,
           const SCO& sconame);

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream& ostream,
           SCO& sconame);

class SCO
{
public:
    explicit SCO(SCONumber number = SCONumber(0),
                 SCOCloneID cloneID = SCOCloneID(0),
                 SCOVersion version = SCOVersion(0))
        : version_(version)
        , number_(number)
        , cloneID_(cloneID)
    {}

    explicit SCO(const std::string& str);

    bool
    asBool() const
    {
        return bool(number_ || version_ || cloneID_);
    }

    bool
    operator==(const SCO& other) const;

    bool
    operator!=(const SCO& other) const;

    static bool
    isSCOString(const std::string& str);

    std::string
    str() const;

    DECLARE_LOGGER("SCO");

    inline void
    version(SCOVersion iver)
    {
        version_ = iver;
    }

    inline SCOVersion
    version() const
    {
        return SCOVersion(version_);
    }

    inline void
    cloneID(SCOCloneID iclone)
    {
        cloneID_ = iclone;
    }

    inline SCOCloneID
    cloneID() const
    {
        return SCOCloneID(cloneID_);
    }

    void
    number(SCONumber inum)
    {
        number_ = inum;
    }

    inline SCONumber
    number() const
    {
        return SCONumber(number_);
    }

    // These will have to be changed
    bool
    operator<(const SCO& in) const
    {
        return bool(((number_ < in.number_) or
                     ((number_ == in.number_) and (version_ < in.version_)) or
                     ((number_ == in.number_) and (version_ == in.version_) and (cloneID_ < in.cloneID_))));
    }

    bool
    operator>(const SCO& in) const
    {
        return bool((number_ > in.number_) or
                    ((number_ == in.number_) and (version_ > in.version_)) or
                    ((number_ == in.number_) and (version_ == in.version_) and (cloneID_ > in.cloneID_)));

    }
    inline void
    incrementNumber(SCONumber inc = SCONumber(1))
    {
        number_ += inc;
    }

    inline void
    incrementVersion(SCOVersion inc = SCOVersion(1))
    {
        version_ += inc;
    }
    inline void
    incrementCloneID(SCOCloneID inc = SCOCloneID(1))
    {
        cloneID_ += inc;
    }

    inline void
    decrementNumber(SCONumber dec = SCONumber(1))
    {
        number_ -= dec;
    }

    inline void
    decrementVersion(SCOVersion dec = SCOVersion(1))
    {
        version_ -= dec;
    }

    inline void
    decrementCloneID(SCOCloneID dec = SCOCloneID(1))
    {
        cloneID_ -= dec;
    }

    static bool
    hasonlyhexdigits(const std::string& str,
                     size_t i1,
                     size_t i2);

    static uint64_t
    gethexnum(const std::string& str,
              size_t i1,
              size_t i2);

private:
    friend class boost::serialization::access;
    friend fungi::IOBaseStream&
    ::volumedriver::operator<<(fungi::IOBaseStream& ostream,
                               const SCO& sconame);

    friend fungi::IOBaseStream&
    ::volumedriver::operator>>(fungi::IOBaseStream& ostream,
                               SCO& sconame);

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int /*version*/)
    {
        uint8_t vers;
        SCONumber num;
        uint8_t id;

        ar & vers;
        ar & num;
        ar & id;

        version_ = vers;
        number_ = num;
        cloneID_ = id;
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned int /*version*/) const
    {
        uint8_t vers = version();
        SCONumber num = number();
        uint8_t id = cloneID();


        ar & vers;
        ar & num;
        ar & id;
    }



    uint8_t version_;
    uint32_t number_;
    uint8_t cloneID_;

} __attribute__((__packed__));

typedef std::list<SCO> SCONameList;

}



BOOST_CLASS_VERSION(volumedriver::SCO,0);

#endif // SCONAME_H_

// Local Variables: **
// mode: c++ **
// End: **
