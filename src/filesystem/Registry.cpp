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

#include "Registry.h"

#include <boost/lexical_cast.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/thread.hpp>

#include <youtils/ArakoonNodeConfig.h>
#include <youtils/ConfigurationReport.h>

namespace volumedriverfs
{

namespace ara = arakoon;
namespace bpt = boost::property_tree;
namespace ip = initialized_params;
namespace yt = youtils;

typedef boost::archive::text_iarchive iarchive_type;
typedef boost::archive::text_oarchive oarchive_type;

Registry::Registry(const bpt::ptree& pt,
                   const RegisterComponent registrate)
    : yt::VolumeDriverComponent(registrate, pt)
    , yt::LockedArakoon(ara::ClusterID(decltype(vregistry_arakoon_cluster_id)(pt).value()),
                        decltype(vregistry_arakoon_cluster_nodes)(pt).value(),
                        ara::Cluster::MaybeMilliSeconds(decltype(vregistry_arakoon_timeout_ms)(pt).value()))
    , vregistry_arakoon_cluster_id(pt)
    , vregistry_arakoon_cluster_nodes(pt)
    , vregistry_arakoon_timeout_ms(pt)
{}

const char*
Registry::componentName() const
{
    return initialized_params::volumeregistry_component_name;
}

void
Registry::update(const bpt::ptree& pt,
                 yt::UpdateReport& rep)
{
#define U(val)                                  \
    val.update(pt, rep)

    U(vregistry_arakoon_cluster_id);
    U(vregistry_arakoon_timeout_ms);

    const auto t(ara::Cluster::MilliSeconds(vregistry_arakoon_timeout_ms.value()));
    timeout(t);

    bool updated = false;
    try
    {
        const decltype(vregistry_arakoon_cluster_nodes) nodes(pt);

        if (nodes.value() != vregistry_arakoon_cluster_nodes.value())
        {
            reconnect(nodes.value(),
                      t);
        }

        updated = true;
    }
    CATCH_STD_ALL_LOG_IGNORE("Failed to update arakoon config");

    if (updated)
    {
        U(vregistry_arakoon_cluster_nodes);
    }
    else
    {
        rep.no_update(vregistry_arakoon_cluster_nodes.name());
    }

#undef U
}

void
Registry::persist(bpt::ptree& pt,
                  const ReportDefault report_default) const
{
#define P(var)                                  \
    (var).persist(pt, report_default);

    P(vregistry_arakoon_cluster_id);
    P(vregistry_arakoon_cluster_nodes);
    P(vregistry_arakoon_timeout_ms);

#undef P
}

bool
Registry::checkConfig(const bpt::ptree& pt,
                      yt::ConfigurationReport& crep) const
{
    const decltype(vregistry_arakoon_cluster_nodes) new_nodes(pt);
    if (new_nodes.value().empty())
    {
        crep.push_back(yt::ConfigurationProblem(new_nodes.name(),
                                                new_nodes.section_name(),
                                                "empty arakoon node configs not permitted"));
        return false;
    }
    else
    {
        return true;
    }
}

}
