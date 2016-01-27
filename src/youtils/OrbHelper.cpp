// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "Assert.h"
#include "Catchers.h"
#include "OrbHelper.h"
#include "OrbParameters.h"

namespace youtils
{
namespace fs = boost::filesystem;

OrbHelper::options_t
OrbHelper::default_options = { { nullptr, nullptr } };


OrbHelper::OrbHelper(const std::string& executable_name,
                     const std::string& orb_id,
                     options_t options)
{
    omniORB::setLogFunction(OrbHelper::logFunction);

    int argc = 1;

    const char* argv[] =  { executable_name.c_str(),
                      0
    };

    orb_ = CORBA::ORB_init(argc,
                           (char**)argv,
                           orb_id.c_str(), // Orb identifier, can we use this??
                           options);
    VERIFY(not CORBA::is_nil(orb_));
}

OrbHelper::~OrbHelper()
{
    try
    {
        if(shutdown_thread_)
        {
            shutdown_thread_->join();
        }

        if(not CORBA::is_nil(orb_))
        {
            orb_->destroy();
        }
    }
    CATCH_STD_ALL_LOG_IGNORE("Failed to clean up OrbHelper");
}

void
OrbHelper::stop()
{
    if(shutdown_thread_)
    {
        shutdown_thread_->join();
    }

    else if(not CORBA::is_nil(orb_))
    {
        shutdown_thread_ =
            std::make_unique<boost::thread>([this]()
                                            {
                                                try
                                                {
                                                    // wait for completion
                                                    orb_->shutdown(true);
                                                }
                                                CATCH_STD_ALL_LOG_IGNORE("Failed to shut down OrbHelper");
                                            });

    }

}

void
OrbHelper::dump_object_to_file(fs::path& file,
                               CORBA::Object_ptr obj)
{
    CORBA::String_var sior(orb_->object_to_string(obj));
    fs::ofstream stream(file);
    stream << (char*)sior << std::endl;
}


CORBA::Object_ptr
OrbHelper::get_object_from_file(fs::path& file)
{
    fs::ifstream stream(file);
    std::string str;
    stream >> str;

    CORBA::Object_var obj = orb_->string_to_object(str.c_str());
    return obj;
}

void
OrbHelper::bindObjectToName(CORBA::Object_ptr objref,
                            const std::string& context_name,
                            const std::string& context_kind,
                            const std::string& object_name,
                            const std::string& object_kind)
{
    LOG_TRACE("binding object to " << context_name
              << context_kind
              << object_name
              << object_kind);


    // Obtain a reference to the root context of the Name service:
    CORBA::Object_var obj;
    obj = orb_->resolve_initial_references("NameService");
    if(CORBA::is_nil(obj))
    {
        LOG_ERROR("Failed to get the NameService");
        throw CouldNotGetNameService("Failed to get the NameService");
    }
    LOG_TRACE("Got the naming service");
    CosNaming::NamingContext_var rootContext;

    try
    {
        rootContext = CosNaming::NamingContext::_narrow(obj);
    }
    catch(CORBA::Exception& e)
    {
        LOG_ERROR("Failed to narrow the root naming context: " << e._name());
        throw CouldNotNarrowToNameService("Failed to narrow the root naming context.");
    }

    if( CORBA::is_nil(rootContext) )
    {
        LOG_ERROR("Failed to narrow the root naming context.");
        throw CouldNotNarrowToNameService("Failed to narrow the root naming context.");
    }

    LOG_TRACE("Success narrowing to root NamingContext");


    CosNaming::Name contextName;
    contextName.length(1);
    contextName[0].id   = context_name.c_str();       // string copied
    contextName[0].kind = context_kind.c_str() ; // string copied
    // Note on kind: The kind field is used to indicate the type
    // of the object. This is to avoid conventions such as that used
    // by files (name.type -- e.g. test.ps = postscript etc.)

    CosNaming::NamingContext_var testContext;
    try
    {
        testContext = rootContext->bind_new_context(contextName);
    }
    catch(CosNaming::NamingContext::AlreadyBound& ex)
    {
        // If the context already exists, this exception will be raised.
        // In this case, just resolve the name and assign testContext
        // to the object returned:
        CORBA::Object_var obj;
        obj = rootContext->resolve(contextName);
        testContext = CosNaming::NamingContext::_narrow(obj);
        if(CORBA::is_nil(testContext))
        {
            LOG_ERROR("Failed to narrow naming context.");
            throw CouldNotNarrowToNamingContext("Failed to narrow naming context.");
        }
    }
    LOG_TRACE("Success binding new context to root NamingContext");

    CosNaming::Name objectName;
    objectName.length(1);
    objectName[0].id   = object_name.c_str();
    objectName[0].kind = object_kind.c_str();

    try
    {

        testContext->bind(objectName, objref);
    }
    catch(CosNaming::NamingContext::AlreadyBound& ex)
    {
        testContext->rebind(objectName, objref);
    }
    LOG_TRACE("Succes binding object to names ")
}

CORBA::Object_ptr
OrbHelper::getObjectReference(const std::string& context_name,
                              const std::string& context_kind,
                              const std::string& object_name,
                              const std::string& object_kind)
{


    // obtain a reference to the root context of the Name service:
    CORBA::Object_var obj;
    obj = orb_->resolve_initial_references("NameService");
    if(CORBA::is_nil(obj))
    {
        LOG_ERROR("Failed to get the NameService");
        throw CouldNotGetNameService("Failed to get the NameService");
    }
    CosNaming::NamingContext_var rootContext;
    try
    {
        rootContext = CosNaming::NamingContext::_narrow(obj);
    }
    catch(CORBA::Exception& e)
    {
        LOG_ERROR("Failed to narrow the root naming context: " << e._name());
        throw CouldNotNarrowToNameService("Failed to narrow the root naming context.");
    }

    if( CORBA::is_nil(rootContext) )
    {
        LOG_ERROR("Failed to narrow the root naming context.");
        throw  CouldNotNarrowToNameService("Failed to narrow the root naming context.");
    }

    // Create a name object, containing the name test/context:
    CosNaming::Name name;
    name.length(2);

    name[0].id   = context_name.c_str();
    name[0].kind = context_kind.c_str();
    name[1].id   = object_name.c_str();
    name[1].kind = object_kind.c_str();
    // Note on kind: The kind field is used to indicate the type
    // of the object. This is to avoid conventions such as that used
    // by files (name.type -- e.g. test.ps = postscript etc.)

    // Resolve the name to an object reference.
    return rootContext->resolve(name);
}
}
