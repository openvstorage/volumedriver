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

}

// Local Variables: **
// mode: c++ **
// End: **
