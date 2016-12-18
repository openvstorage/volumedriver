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

#include "BackendConfig.h"
#include "BackendConnectionManager.h"
#include "BackendInterface.h"
#include "BackendTestSetup.h"

#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/regex.hpp>
#include <boost/scope_exit.hpp>

#include <gtest/gtest.h>

#include <youtils/Assert.h>
// #include <youtils/FileUtils.h>
#include <youtils/MainHelper.h>
#include "TestWithBackendMainHelper.h"

namespace backend
{
void
BackendTestSetup::initialize_connection_manager()
{

    ASSERT(TestWithBackendMainHelper::backendConfig());

    boost::property_tree::ptree pt;
    TestWithBackendMainHelper::backendConfig()->persist_internal(pt,
                                                                 ReportDefault::F);

    cm_ = BackendConnectionManager::create(pt,
                                           RegisterComponent::F);
}

void
BackendTestSetup::uninitialize_connection_manager()
{
    cm_.reset();
}


const BackendConfig&
BackendTestSetup::backend_config()
{
    return *TestWithBackendMainHelper::backendConfig();
}

void
BackendTestSetup::deleteNamespace(const Namespace& nspace)
{
    BackendConnectionInterfacePtr conn(cm_->getConnection());
    conn->deleteNamespace(nspace);

}

void
BackendTestSetup::clearNamespace(const Namespace& nspace)
{
    BackendConnectionInterfacePtr conn(cm_->getConnection());
    conn->clearNamespace(nspace);
}

void
BackendTestSetup::backendRegex(const Namespace& nspace,
                               const std::string& pattern,
                               std::list<std::string>& res)
{
    const boost::regex regex(pattern);
    std::list<std::string> objs;
    BackendConnectionInterfacePtr conn(cm_->getConnection());

    conn->listObjects(nspace,
                      objs);

    BOOST_FOREACH(const std::string& o, objs)
    {
        if (regex_match(o, regex))
        {
            res.push_back(o);
        }
    }
}

BackendInterfacePtr
BackendTestSetup::createBackendInterface(const Namespace& nspace)
{
    return cm_->newBackendInterface(nspace);
}


bool
BackendTestSetup::streamingSupport()
{
    TODO("Y42: Find best way to fix this");
    return not (backend_config().backend_type.value() == BackendType::ALBA or
                backend_config().backend_type.value() == BackendType::S3);
}

std::shared_ptr<ConnectionPool>
BackendTestSetup::connection_manager_pool(BackendConnectionManager& cm,
                                          const Namespace& nspace)
{
    return cm.pool_(nspace);
}

}

// Local Variables: **
// mode: c++ **
// End: **
