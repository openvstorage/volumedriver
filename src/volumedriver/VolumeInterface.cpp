// Copyright (C) 2018 iNuron NV
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

#include "VolumeInterface.h"
#include "VolManager.h"

namespace volumedriver
{

namespace be = backend;
namespace yt = youtils;

boost::shared_ptr<be::Condition>
VolumeInterface::claim_namespace(const be::Namespace& nspace,
                                 const OwnerTag owner_tag)
{
    const std::string& name(owner_tag_backend_name());
    const fs::path p(yt::FileUtils::create_temp_file_in_temp_dir(nspace.str() + "." + name));
    ALWAYS_CLEANUP_FILE(p);

    fs::ofstream(p) << owner_tag;

    be::BackendInterfacePtr bi(VolManager::get()->createBackendInterface(nspace));

    try
    {
        return boost::make_shared<be::Condition>(name,
                                                 bi->write_tag(p,
                                                               name,
                                                               nullptr,
                                                               OverwriteObject::T));
    }
    catch (be::BackendNotImplementedException&)
    {
        return nullptr;
    }
}

void
VolumeInterface::verify_namespace_ownership()
{
    boost::shared_ptr<be::Condition> cond(backend_write_condition());

    if (cond)
    {
        std::unique_ptr<yt::UniqueObjectTag>
            t(getBackendInterface()->get_tag(cond->object_name()));
        VERIFY(t);
        if (*t != cond->object_tag())
        {
            LOG_ERROR(getName() << ": owner tag hash mismatch, expected " <<
                      cond->object_tag() << ", got " << *t);
            halt();
            throw OwnerTagMismatchException("owner tag hash mismatch");
        }
    }
    else
    {
        LOG_WARN(getName() << ": namespace not claimed?");
    }
}

}
