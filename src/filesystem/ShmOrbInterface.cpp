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

#include <boost/exception_ptr.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>

#include <youtils/OrbHelper.h>
#include <filesystem/ObjectRouter.h>
#include <filesystem/Registry.h>

#include "ShmIdlInterface.h"
#include "ShmInterface.h"
#include "ShmVolumeDriverHandler.h"
#include "ShmOrbInterface.h"

namespace volumedriverfs
{

namespace bi = boost::interprocess;
namespace bpt = boost::property_tree;
namespace yt = youtils;
namespace ip = initialized_params;

void
ShmOrbInterface::run(std::promise<void> promise)
{
    using youtils::OrbHelper;
    bool initialized = false;

    try
    {
        //OrbHelper orb_helper("volumedriverfs_shm_server");
        CORBA::Object_var obj = orb_helper_.orb()->resolve_initial_references("RootPOA");
        PortableServer::POA_var poa = PortableServer::POA::_narrow(obj);

        typedef ShmInterface<ShmVolumeDriverHandler> ShmVolumeFactory;
        typedef ShmVolumeDriverHandler::construction_args handler_args_t;
        handler_args_t handler_args =
        {
            fs_
        };

        POA_ShmIdlInterface::VolumeFactory_tie<ShmVolumeFactory>
            myecho(new ShmVolumeFactory(orb_helper_, handler_args));

        PortableServer::ObjectId_var myecho_id = poa->activate_object(&myecho);

        CORBA::Object_var obj1 = myecho._this();
        PortableServer::POAManager_var pman = poa->the_POAManager();
        pman->activate();

        try
        {
            orb_helper_.bindObjectToName(obj1,
                                         vd_context_name,
                                         vd_context_kind,
                                         vd_object_name,
                                         vd_object_kind);
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR("Exception binding to name: " << EWHAT);
                poa->deactivate_object(myecho_id);
                pman->deactivate(false, true);
                throw;
            });

        initialized = true;
        promise.set_value();
        LOG_INFO("Starting SHM server");
        orb_helper_.orb()->run();
    }
    catch (CORBA::Exception& e)
    {
        LOG_ERROR("Caught CORBA::Exception " << e._name());
        if (not initialized)
        {
            promise.set_exception(std::current_exception());
        }
        throw;
    }
    catch (omniORB::fatalException& e)
    {
        LOG_ERROR("Caught omniORB::fatalException, file: " << e.file()
                  << " line: " << e.line()
                  << " mesg: " << e.errmsg());
        if (not initialized)
        {
            promise.set_exception(std::current_exception());
        }
        throw;
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("Caught exception: " << EWHAT);
            if (not initialized)
            {
                promise.set_exception(std::current_exception());
            }
            throw;
        });
}

void
ShmOrbInterface::stop_all_and_exit()
{
    try
    {
        CORBA::Object_var obj = orb_helper_.getObjectReference(vd_context_name,
                                                               vd_context_kind,
                                                               vd_object_name,
                                                               vd_object_kind);
        ShmIdlInterface::VolumeFactory_var volumefactory_ref_ =
            ShmIdlInterface::VolumeFactory::_narrow(obj);
        volumefactory_ref_->stop_all_and_exit();
    }
    catch (CORBA::SystemException& ex)
    {
        LOG_FATAL("Caught CORBA::SystemException " << ex._name());
    }
    catch (CORBA::Exception& ex)
    {
        LOG_FATAL("Caught CORBA::Exception " << ex._name());
    }
    catch (omniORB::fatalException& fe)
    {
        LOG_FATAL("Caught omniORB::fatalException " << fe.file()
                  << " line: " << fe.line()
                  << " mesg: " << fe.errmsg());
    }
}

ShmOrbInterface::~ShmOrbInterface()
{
    try
    {
        bi::shared_memory_object::remove(ShmSegmentDetails::Name());
    }
    catch (bi::interprocess_exception&)
    {
        LOG_FATAL("SHM: Cannot remove shared memory segment");
    }
}

const char*
ShmOrbInterface::componentName() const
{
    return ip::shm_interface_component_name;
}

void
ShmOrbInterface::update(const bpt::ptree& pt,
                        yt::UpdateReport& rep)
{
#define U(var)                                  \
        (var).update(pt, rep)
    U(shm_region_size);
#undef U
}

void
ShmOrbInterface::persist(bpt::ptree& pt,
                         const ReportDefault report_default) const
{
#define P(var)                                  \
        (var).persist(pt, report_default)
    P(shm_region_size);
#undef U
}

bool
ShmOrbInterface::checkConfig(const bpt::ptree& /*pt*/,
                             yt::ConfigurationReport& /*crep*/) const
{
    bool res = true;
    return res;
}

}
