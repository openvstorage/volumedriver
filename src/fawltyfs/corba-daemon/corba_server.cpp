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

// eg3_tieimpl.cc - This example is similar to eg3_impl.cc except that
//                  the tie implementation skeleton is used.
//
//               This is the object implementation.
//
// Usage: eg3_tieimpl
//
//        On startup, the object reference is registered with the
//        COS naming service. The client uses the naming service to
//        locate this object.
//
//        The name which the object is bound to is as follows:
//              root  [context]
//               |
//              test  [context] kind [my_context]
//               |
//              Echo  [object]  kind [Object]
//
#include <youtils/Main.h>

#include <fawlty.hh>
#include <boost/filesystem/fstream.hpp>
#include "FawltyImplementation.h"
#include "Literals.h"
#include <iostream>

namespace
{

using namespace std;
using namespace Fawlty;

namespace fs = boost::filesystem;
namespace po = boost::program_options;

class CorbaServer : public youtils::MainHelper
{
public:
    CorbaServer(int argc,
                char** argv)
        : MainHelper(argc, argv)
        , stop_(false)
        , options_("Standard Options")
    {
        options_.add_options()
            ("use-naming-service",
             po::value<bool>(&use_naming_service_)->default_value(true),
             "publish the fawlty object in the naming service, otherwise the object reft is dumped to a file")
            ("ior-file",
             po::value<std::string>(&ior_file_)->default_value("fawlty.ior"),
             "filename to dump the ior of the fawlty object to");

    }

    virtual void
    log_extra_help(std::ostream& strm)
    {
        strm << options_;

    }

    void
    parse_command_line_arguments()
    {
        parse_unparsed_options(options_,
                               youtils::AllowUnregisteredOptions::T,
                               vm_);
    }

    virtual void
    setup_logging()
    {
        MainHelper::setup_logging();
    }

    int
    run()
    {
        try
        {
            int argc = 1;

            const char* argv[] = {
                "CorbaClientTest",
                0
            };


            CORBA::ORB_var orb = CORBA::ORB_init(argc, (char**)argv);

            CORBA::Object_var obj = orb->resolve_initial_references("RootPOA");
            PortableServer::POA_var poa = PortableServer::POA::_narrow(obj);

            // Note that the <myecho> tie object is constructed on the stack
            // here. It will delete its implementation (myimpl) when it it
            // itself destroyed (when it goes out of scope).  It is essential
            // however to ensure that such servants are not deleted whilst
            // still activated.
            //
            // Tie objects can of course be allocated on the heap using new,
            // in which case they are deleted when their reference count
            // becomes zero, as with any other servant object.

            POA_Fawlty::FileSystemFactory_tie<FawltyImplementation> myecho(new FawltyImplementation(stop_,
                                                                                                    poa));

            PortableServer::ObjectId_var myechoid = poa->activate_object(&myecho);

            if(use_naming_service_)
            {
                obj = myecho._this();
                if(not bindObjectToName(orb, obj))
                    return 1;
            }
            else
            {
                obj = myecho._this();
                CORBA::String_var sior(orb->object_to_string(obj));
                fs::ofstream s(ior_file_);
                s << (char*)sior << endl;

            }

            PortableServer::POAManager_var pman = poa->the_POAManager();
            pman->activate();


            while(not stop_)
            {
                omni_thread::sleep(1);
                if (orb->work_pending())
                {
                    cout << "work pending" << endl;
                    orb->perform_work();
                }
             }
            orb->destroy();
            return 0;

        }
        catch(CORBA::SystemException& ex)
        {
            LOG_FATAL("Caught CORBA::" << ex._name());
        }
        catch(CORBA::Exception& ex)
        {
            LOG_FATAL("Caught CORBA::Exception: " << ex._name());
        }
        catch(omniORB::fatalException& fe)
        {
            LOG_FATAL("Caught omniORB::fatalException:  file: " << fe.file()
                      << "  line: " << fe.line()
                      << "  mesg: " << fe.errmsg());
        }
        return 1;
    }

private:
    bool stop_;
    po::options_description options_;
    bool use_naming_service_;
    std::string ior_file_;


    CORBA::Boolean
    bindObjectToName(CORBA::ORB_ptr orb,
                     CORBA::Object_ptr objref)
    {
        CosNaming::NamingContext_var rootContext;

        try {
            // Obtain a reference to the root context of the Name service:
            CORBA::Object_var obj;
            obj = orb->resolve_initial_references("NameService");

            // Narrow the reference returned.
            rootContext = CosNaming::NamingContext::_narrow(obj);
            if( CORBA::is_nil(rootContext) ) {
                cerr << "Failed to narrow the root naming context." << endl;
                return 0;
            }
        }
        catch (CORBA::NO_RESOURCES&) {
            cerr << "Caught NO_RESOURCES exception. You must configure omniORB "
                 << "with the location" << endl
                 << "of the naming service." << endl;
            return 0;
        }
        catch (CORBA::ORB::InvalidName&) {
            // This should not happen!
            cerr << "Service required is invalid [does not exist]." << endl;
            return 0;
        }

        try {
            // Bind a context called "test" to the root context:

            CosNaming::Name contextName;
            contextName.length(1);
            contextName[0].id   = (const char*) TEST_NAME;       // string copied
            contextName[0].kind = (const char*) TEST_KIND; // string copied
            // Note on kind: The kind field is used to indicate the type
            // of the object. This is to avoid conventions such as that used
            // by files (name.type -- e.g. test.ps = postscript etc.)

            CosNaming::NamingContext_var testContext;
            try {
                // Bind the context to root.
                testContext = rootContext->bind_new_context(contextName);
            }
            catch(CosNaming::NamingContext::AlreadyBound& ex) {
                // If the context already exists, this exception will be raised.
                // In this case, just resolve the name and assign testContext
                // to the object returned:
                CORBA::Object_var obj;
                obj = rootContext->resolve(contextName);
                testContext = CosNaming::NamingContext::_narrow(obj);
                if( CORBA::is_nil(testContext) ) {
                    cerr << "Failed to narrow naming context." << endl;
                    return 0;
                }
            }

            // Bind objref with name Echo to the testContext:
            CosNaming::Name objectName;
            objectName.length(1);
            objectName[0].id   = (const char*) OBJECT_NAME;   // string copied
            objectName[0].kind = (const char*) OBJECT_KIND; // string copied

            try
            {
                testContext->bind(objectName, objref);
            }
            catch(CosNaming::NamingContext::AlreadyBound& ex)
            {
                testContext->rebind(objectName, objref);
            }
            // Note: Using rebind() will overwrite any Object previously bound
            //       to /test/Echo with obj.
            //       Alternatively, bind() can be used, which will raise a
            //       CosNaming::NamingContext::AlreadyBound exception if the name
            //       supplied is already bound to an object.

            // Amendment: When using OrbixNames, it is necessary to first try bind
            // and then rebind, as rebind on it's own will throw a NotFoundexception if
            // the Name has not already been bound. [This is incorrect behaviour -
            // it should just bind].
        }
        catch(CORBA::TRANSIENT& ex) {
            cerr << "Caught system exception TRANSIENT -- unable to contact the "
                 << "naming service." << endl
                 << "Make sure the naming server is running and that omniORB is "
                 << "configured correctly." << endl;

            return 0;
        }
        catch(CORBA::SystemException& ex) {
            cerr << "Caught a CORBA::" << ex._name()
                 << " while using the naming service." << endl;
            return 0;
        }
        return 1;
    }
};

}

MAIN(CorbaServer);
