// Copyright 2015 Open vStorage NV
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
