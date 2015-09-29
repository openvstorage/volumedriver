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

#include "VolumeFailOverCacheWriterMap.h"
#include <youtils/Assert.h>
namespace failovercache
{

using namespace volumedriver;

VolumeFailOverCacheWriterMap::VolumeFailOverCacheWriterMap(const fs::path& root)
    :root_(root)
{}


VolumeFailOverCacheWriterMap::~VolumeFailOverCacheWriterMap()
{
    for(iterator i = begin(); i != end(); ++i)
    {
        delete i->second;
    }
}

FailOverCacheWriter*
VolumeFailOverCacheWriterMap::lookup(const std::string& namespaceName,
                                     const ClusterSize& cluster_size)
{
    iterator i = find(namespaceName);
    if(i == end())
    {
        FailOverCacheWriter* c =  new FailOverCacheWriter(root_,
                                                          namespaceName,
                                                          cluster_size);
        operator[](namespaceName) = c;
        c->register_();
        return c;
    }
    else
    {
        FailOverCacheWriter* foc = i->second;
        foc->setFirstCommandMustBeGetEntries();
        foc->register_();
        return foc;
    }
}

void
VolumeFailOverCacheWriterMap::remove(const FailOverCacheWriter* writer)
{
    const_iterator it = find(writer->getNamespace());
    DEBUG_CHECK(it != end());
    if(it == end())
    {
        LOG_WARN("Got a remove for namespace " << writer->getNamespace() << " which I don't know");
        return;
    }

    VERIFY(it->second == writer);
    erase(it);
}

}

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
