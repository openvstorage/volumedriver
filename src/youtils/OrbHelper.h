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

#ifndef ORB_HELPER_H_
#define ORB_HELPER_H_
#include <string>
#include <boost/filesystem/fstream.hpp>
#include <omniORB4/CORBA.h>
#include <boost/thread.hpp>
#include "Logging.h"
#include "IOException.h"

namespace youtils
{
MAKE_EXCEPTION(OrbHelperException, fungi::IOException);
MAKE_EXCEPTION(CouldNotBindObject, OrbHelperException);
MAKE_EXCEPTION(CouldNotGetNameService, OrbHelperException);
MAKE_EXCEPTION(CouldNotNarrowToNameService, OrbHelperException);
MAKE_EXCEPTION(CouldNotNarrowToNamingContext, OrbHelperException);



class OrbHelper
{
public:
    typedef const char* option_t[2];

    typedef option_t options_t[];

    static options_t default_options;

    OrbHelper(const std::string& executable_name,
              const std::string& orb_id = "",
              options_t options = default_options );

    // Has to be enhanced to update it's args when parsing code picks them off.

    ~OrbHelper();

    void
    dump_object_to_file(boost::filesystem::path&,
                        CORBA::Object_ptr p);

    CORBA::Object_ptr
    get_object_from_file(boost::filesystem::path& );


    void
    bindObjectToName(CORBA::Object_ptr objref,
                     const std::string& context_name,
                     const std::string& context_kind,
                     const std::string& object_name,
                     const std::string& object_kind);

    CORBA::Object_ptr
    getObjectReference(const std::string& context_name,
                       const std::string& context_kind,
                       const std::string& object_name,
                       const std::string& object_kind);

    void
    stop();

    CORBA::ORB_var&
    orb()
    {
        return orb_;
    }


private:
    DECLARE_LOGGER("OrbHelper");

    CORBA::ORB_var orb_;

    std::unique_ptr<boost::thread> shutdown_thread_;

    static void
    logFunction(const char* message)
    {
        LOG_INFO(message);
    }

};
}

#endif // ORB_HELPER_H_
