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

#include <boost/python.hpp>
#include <backend-python/ConnectionInterface.h>
#include <boost/python/tuple.hpp>
#include <boost/foreach.hpp>

#include "SCOAccessDataInfo.h"

namespace toolcut
{

SCOAccessDataInfo::SCOAccessDataInfo(const fs::path& p)
    : sad_(vd::SCOAccessDataPersistor::deserialize(p))
{
}

SCOAccessDataInfo::SCOAccessDataInfo(const fs::path& dst_path,
                                     bpy::object& backend,
                                     const std::string& nspace)
    : sad_(nullptr)
{

    backend.attr("read")(nspace,
                         dst_path.string(),
                         vd::SCOAccessDataPersistor::backend_name,
                         true);
    sad_ = vd::SCOAccessDataPersistor::deserialize(dst_path);
}

std::string
SCOAccessDataInfo::getNamespace() const
{
    return sad_->getNamespace().str();
}

float
SCOAccessDataInfo::getReadActivity() const
{
    return sad_->read_activity();
}

bpy::list
SCOAccessDataInfo::getEntries() const
{
    bpy::list res;

    BOOST_FOREACH(const vd::SCOAccessData::EntryType& e, sad_->getVector())
    {
        res.append(bpy::make_tuple(e.first.str(), e.second));
    }

    return res;
}

}

// Local Variables: **
// mode: c++ **
// End: **
