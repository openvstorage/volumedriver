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

#ifndef NDEBUG


#include "ClusterLocation.h"
#include "DebugPrint.h"
#include "NSIDMap.h"
#include "Types.h"
#include "VolumeConfig.h"

#include <stdio.h>
#include <iostream>
#include <sstream>

#include <boost/property_tree/info_parser.hpp>

// Enhance this by passing stream &indentation -> prettyprinter
namespace debug
{

void
DumpPropertyTree(const boost::property_tree::ptree& pt)
{
    boost::property_tree::write_info(std::cerr, pt);
}

void
DPVolumeConfig(const volumedriver::VolumeConfig* v)
{
    fprintf(stderr,"id: %s\n", v->id_.c_str());

    if(v->parent())
    {
        fprintf(stderr,
                "parent_ns: %s\n",
                v->parent()->nspace.c_str());
        fprintf(stderr,
                "parent_snap: %s\n", v->parent()->snapshot.c_str());
    }

    fprintf(stderr, "ns: %s\n", v->getNS().c_str());
    //    fprintf(stderr, "tlog_path: %s\n", v->tlog_path);
    {
        std::stringstream ss;
        ss << v->lba_count();
        fprintf(stderr, "lba_cnt: %s\n", ss.str().c_str());
    }
}

void
DPClusterLocation(const volumedriver::ClusterLocation* c)
{
    std::cerr << c;
}

void
DumpStringStream(std::stringstream& ss)
{
    std::cerr << ss.str() << std::endl;
}

}

#endif // NDEBUG
