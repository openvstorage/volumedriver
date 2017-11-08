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

#include <boost/python/module.hpp>

#include <youtils/Gcrypt.h>
#include <youtils/Logger.h>

#include <youtils/python/BuildInfoAdapter.h>
#include <youtils/python/LoggingAdapter.h>

#include <backend/python/ConnectionManagerAdapter.h>
#include <backend/python/Exceptions.h>
#include <backend/python/InterfaceAdapter.h>

namespace bepy = backend::python;
namespace ypy = youtils::python;
namespace yt = youtils;

BOOST_PYTHON_MODULE(Backend)
{
    yt::Gcrypt::init_gcrypt();
    ypy::register_once<ypy::LoggingAdapter>();
    ypy::register_once<ypy::BuildInfoAdapter>();
    ypy::register_once<bepy::ConnectionManagerAdapter>();
    ypy::register_once<bepy::InterfaceAdapter>();
    ypy::register_once<bepy::Exceptions>();
}

// Local Variables: **
// mode: c++ **
// End: **
