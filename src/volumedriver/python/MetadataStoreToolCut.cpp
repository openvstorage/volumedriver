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

#include "MetadataStoreToolCut.h"

#include "../ClusterLocationAndHash.h"

#include <boost/python/tuple.hpp>

namespace volumedriver
{

namespace python
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
        obj_(addr, hsh.clusterLocation);
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

}

// Local Variables: **
// mode: c++ **
// End: **
