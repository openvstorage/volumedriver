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
            ostr_ << ", " << clh.weed;
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
