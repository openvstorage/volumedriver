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

#include "EtcdConfigFetcher.h"
#include "EtcdReply.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <etcdcpp/etcd.hpp>

namespace youtils
{

namespace bpt = boost::property_tree;

namespace
{

std::string
parse_config(const EtcdReply::Records& recs)
{
    bpt::ptree pt;
    std::stringstream ss;

    for (const auto& rec: recs)
    {
        if (not rec.second.dir)
        {
            bpt::ptree child;
            if (rec.second.value)
            {
                std::stringstream st;
                st << *rec.second.value;
                bpt::read_json(st, child);
            }
            std::string tmp_key = rec.second.key;
            pt.add_child(tmp_key.substr(tmp_key.find_last_of("/") + 1),
                         child);
        }
    }
    bpt::write_json(ss, pt);
    return ss.str();
}

}

bpt::ptree
EtcdConfigFetcher::operator()(VerifyConfig verify_config)
{
    LOG_INFO("Fetching config from " << url_);

    const boost::optional<uint16_t> port(url_.port());
    VERIFY(port);

    etcd::Client<EtcdReply> client(url_.host(),
                                   *port);

    const EtcdReply reply(client.Get(url_.path(),
                                     true));
    const boost::optional<EtcdReply::Error> err(reply.error());
    if (err)
    {
        LOG_ERROR("Etcd reported error retrieving " << url_ <<
                  ": code " << err->error_code <<
                  ", message " << err->message <<
                  ", cause " << err->cause);
        throw Exception("Failed to retrieve config from etcd");
    }

    const boost::optional<EtcdReply::Node> node(reply.node());
    if (not node)
    {
        LOG_ERROR("Failed to find desired node in " << url_);
        throw Exception("Failed to find node in etcd");
    }

    const EtcdReply::Records recs(reply.records());
    if (recs.size() < 2)
    {
        LOG_ERROR("Failed to find config subsections under " << url_);
        throw Exception("Failed to find config subsections");
    }

    std::stringstream ss(parse_config(recs));
    return VolumeDriverComponent::read_config(ss,
                                              verify_config);
}

}
