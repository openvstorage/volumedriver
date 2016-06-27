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

#ifndef DTL_ACCEPTOR_H_
#define DTL_ACCEPTOR_H_

#include "DtlProtocol.h"
#include "BackendFactory.h"

#include "../DtlStreamers.h"

#include <unordered_map>

#include <boost/thread/mutex.hpp>

#include "fungilib/ProtocolFactory.h"

namespace volumedrivertest
{

class FailOverCacheTestContext;

}

namespace distributed_transaction_log
{

class Acceptor
    : public fungi::ProtocolFactory
{
    friend class volumedrivertest::FailOverCacheTestContext;

public:
    explicit Acceptor(const boost::optional<boost::filesystem::path>& root);

    virtual ~Acceptor();

    virtual fungi::Protocol*
    createProtocol(std::unique_ptr<fungi::Socket>,
                   fungi::SocketServer& parentServer) override final;

    virtual const char *
    getName() const override final
    {
        return "Acceptor";
    }

    void
    remove(Backend&);

    using BackendPtr = std::shared_ptr<Backend>;

    BackendPtr
    lookup(const volumedriver::CommandData<volumedriver::Register>&);

    void
    removeProtocol(DtlProtocol* prot)
    {
        boost::lock_guard<decltype(mutex_)> g(mutex_);
        protocols.remove(prot);
    }

private:
    DECLARE_LOGGER("Acceptor");

    // protects the map / protocols.
    boost::mutex mutex_;

    using Map = std::unordered_map<std::string, BackendPtr>;
    Map map_;

    std::list<DtlProtocol*> protocols;
    BackendFactory factory_;

    // for use by testers
    BackendPtr
    find_backend_(const std::string&);
};

}
#endif // DTL_ACCEPTOR_H_

// Local Variables: **
// mode : c++ **
// End: **
