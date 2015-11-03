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
