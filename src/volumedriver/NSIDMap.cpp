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

#include "NSIDMap.h"

namespace volumedriver
{
NSIDMap::NSIDMap()
    : std::vector<BackendInterfacePtr>(max_size())
{}

NSIDMap::NSIDMap(NSIDMap&& in_other)
{
    for(unsigned i = 0; i < max_size(); ++i)
    {
        push_back(std::move(in_other[i]));
    }
}

NSIDMap&
NSIDMap::operator=(NSIDMap&& in_other)
{
    if(this != &in_other)
    {
        clear();
        for(unsigned i = 0; i < max_size(); ++i)
        {
            push_back(std::move(in_other[i]));
        }
    }
    return *this;
}

size_t
NSIDMap::size() const
{
    for(size_t i = 0; i < max_size(); ++i)
    {
        if(not get(i))
        {
            return i;
        }
    }
    return max_size();
}

const BackendInterfacePtr&
NSIDMap::get(const SCOCloneID id) const
{
    return operator[](id);
}

const BackendInterfacePtr&
NSIDMap::get(const uint8_t id) const
{
    return operator[](id);
}

void
NSIDMap::set(const SCOCloneID id,
             BackendInterfacePtr bi)
{
    ASSERT(not operator[](id));
    vector<BackendInterfacePtr>::operator[](id) = std::move(bi);
}

void
NSIDMap::set(const uint8_t id,
             BackendInterfacePtr bi)
{
    ASSERT(not operator[](id));
    vector<BackendInterfacePtr>::operator[](id) = std::move(bi);
}

bool
NSIDMap::empty()
{
    for(size_t i = 0; i < max_size(); ++i)
    {
        if(get(i))
        {
            return false;

        }
    }
    return true;
}

SCOCloneID
NSIDMap::getCloneID(const Namespace& ns) const
{
    for(unsigned i = 0; i < max_size(); ++i)
    {
        const BackendInterfacePtr& ptr = vector<BackendInterfacePtr>::operator[](i);
        if(ptr)
        {
            if(ptr->getNS() == ns)
            {
                return SCOCloneID(i);
            }
        }
        else
        {
            break;
        }
    }
    LOG_ERROR("Request for SCOCloneID of namespace " <<
              ns << " that is not in the nsidmap");
    throw fungi::IOException("Could not find SCOCloneID for namespace");
}

}

// Local Variables: **
// mode: c++ **
// End: **
