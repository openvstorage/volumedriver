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

#include "ArakoonLockStoreBuilder.h"
#include "BackendLockStoreBuilder.h"
#include "LockStoreFactory.h"
#include "LockStoreType.h"

#include <youtils/Assert.h>

namespace volumedriver
{

namespace be = backend;
namespace bpt = boost::property_tree;
namespace yt = youtils;

namespace
{

std::unique_ptr<LockStoreBuilder>
make_builder(LockStoreType t,
             const bpt::ptree& pt,
             be::BackendConnectionManagerPtr cm)
{
    switch (t)
    {
    case LockStoreType::Arakoon:
        return std::unique_ptr<LockStoreBuilder>(new ArakoonLockStoreBuilder(pt));
    case LockStoreType::Backend:
        return std::unique_ptr<LockStoreBuilder>(new BackendLockStoreBuilder(pt,
                                                                             cm));
    }

    return nullptr;
}

}

LockStoreFactory::LockStoreFactory(const bpt::ptree& pt,
                                   const RegisterComponent registerizle,
                                   be::BackendConnectionManagerPtr cm)
    : LockStoreBuilder(pt,
                       registerizle)
    , dls_type(pt)
    , builder_(make_builder(dls_type.value(),
                            pt,
                            cm))
{
    LOG_INFO("using LockStoreType " << dls_type.value());
    VERIFY(builder_ != nullptr);
}

void
LockStoreFactory::update(const bpt::ptree& pt,
                         yt::UpdateReport& urep)
{
    dls_type.update(pt,
                    urep);

    builder_->update(pt,
                     urep);
}

void
LockStoreFactory::persist(bpt::ptree& pt,
                          const ReportDefault report_default) const
{
    dls_type.persist(pt,
                     report_default);
    builder_->persist(pt,
                      report_default);
}

bool
LockStoreFactory::checkConfig(const bpt::ptree& pt,
                              yt::ConfigurationReport& crep) const
{
    return builder_->checkConfig(pt,
                                 crep);
}

}
