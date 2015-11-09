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
