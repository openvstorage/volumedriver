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
ShmOrbInterface::run()
{
    using youtils::OrbHelper;
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
        catch(std::exception& e)
        {
            LOG_FATAL("Exception binding to Name: " << e.what());
            poa->deactivate_object(myecho_id);
            pman->deactivate(false, true);
            throw;
        }
        catch(...)
        {
            LOG_FATAL("Unknown exception binding to Name");
            poa->deactivate_object(myecho_id);
            pman->deactivate(false, true);
            throw;
        }

        LOG_INFO("Starting SHM server");
        orb_helper_.orb()->run();
    }
    catch (CORBA::SystemException& ex)
    {
        LOG_FATAL("Caught CORBA::SystemException " << ex._name());
        throw;

    }
    catch (CORBA::Exception& ex)
    {
        LOG_FATAL("Caught CORBA::Exception " << ex._name());
        throw;
    }
    catch (omniORB::fatalException& fe)
    {
        LOG_FATAL("Caught omniORB::fatalException " << fe.file()
                  << " line: " << fe.line()
                  << " mesg: " << fe.errmsg());
        throw;
    }
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
