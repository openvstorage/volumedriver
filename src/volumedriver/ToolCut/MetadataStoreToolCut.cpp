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

#include "MetadataStoreToolCut.h"
#include "ClusterLocationToolCut.h"

#include "../ClusterLocationAndHash.h"

#include <boost/python/tuple.hpp>

namespace toolcut
{

using namespace volumedriver;

class PythonCallBack
    : public MetaDataStoreFunctor
{
public:
    PythonCallBack(boost::python::object& obj)
        : obj_(obj)
    {}

    virtual void
    operator()(ClusterAddress addr, const ClusterLocationAndHash& hsh)
    {
        obj_(addr, ClusterLocationToolCut(hsh.clusterLocation));
    }

private:
    boost::python::object& obj_;
};

MetadataStoreToolCut::MetadataStoreToolCut(const std::string file)
{
    (void) file;
    THROW_UNLESS(0 == "This needs to be adapted to the all new CachedMetaDataStore");
}

const std::string
MetadataStoreToolCut::readCluster(const uint64_t address)
{
    ClusterLocationAndHash clhs;
    md_store_->readCluster(address,
                           clhs);
    std::stringstream ss;
    ss << clhs.clusterLocation;
    return ss.str();
}

void
MetadataStoreToolCut::forEach(boost::python::object& object,
                              const volumedriver::ClusterAddress max_address)
{
    PythonCallBack pcb(object);
    md_store_->for_each(pcb,
                        max_address);
}

boost::python::dict
MetadataStoreToolCut::getStats()
{
    boost::python::dict result;
    return result;
}

}

// Local Variables: **
// mode: c++ **
// End: **
