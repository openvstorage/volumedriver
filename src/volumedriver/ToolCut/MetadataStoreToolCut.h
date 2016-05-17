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
