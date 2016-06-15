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
