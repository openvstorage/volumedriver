// Copyright 2015 Open vStorage NV
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

#ifndef _METADATASTORE_TOOLCUT_H_
#define _METADATASTORE_TOOLCUT_H_

#include "../MetaDataStoreInterface.h"
#include "../Types.h"

#include <boost/python/list.hpp>
#include <boost/python/dict.hpp>
#include <boost/python/object.hpp>

namespace toolcut
{

class MetadataStoreToolCut
{
public:
    MetadataStoreToolCut(const std::string file);

    const std::string
    readCluster(const volumedriver::ClusterAddress i_addr);

    void
    forEach(boost::python::object&,
            const volumedriver::ClusterAddress max_address);

    boost::python::dict
    getStats();

private:
    volumedriver::MetaDataStoreInterface* md_store_;
};
}


#endif

// Local Variables: **
// mode: c++ **
// End: **
