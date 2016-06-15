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

#include "MetaDataStoreDebug.h"
#include "ClusterLocationAndHash.h"

namespace volumedriver
{

namespace
{
struct DumpMD : public MetaDataStoreFunctor
{
    DumpMD(std::ostream& ostr,
           bool dump_weed)
        : ostr_(ostr)
        , dump_weed_(dump_weed)
    {}

    void
    operator()(ClusterAddress ca,
               const ClusterLocationAndHash& clh)
    {
        ostr_ << ca << ": " << clh.clusterLocation;

        if(dump_weed_)
        {
            //cnanakos: handle better?
#ifdef ENABLE_MD5_HASH
            ostr_ << ", " << clh.weed();
#endif
        }
        ostr_ << std::endl;
    }

    std::ostream& ostr_;
    bool dump_weed_;
};

}


void
dump_mdstore(MetaDataStoreInterface& mdstore,
             ClusterAddress max_address,
             std::ostream& out,
             bool with_hash)
{
    DumpMD md_dumper(out,
                     with_hash);
    mdstore.for_each(md_dumper,
                     max_address);

}

}
