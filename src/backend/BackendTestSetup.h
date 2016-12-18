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

#ifndef BACKEND_TEST_SETUP_H_
#define BACKEND_TEST_SETUP_H_

#include "BackendConnectionManager.h"
#include "BackendConfig.h"

#include <loki/Typelist.h>
#include <youtils/Logging.h>
#include <gtest/gtest.h>
#include <youtils/UUID.h>

namespace backend
{

namespace yt = youtils;

class BackendTestSetup
{
public:
    BackendTestSetup()
    {}

    BackendTestSetup(const BackendTestSetup&) = delete;

    BackendTestSetup&
    operator=(const BackendTestSetup&) = delete;


    class WithRandomNamespace
    {
    public:
        WithRandomNamespace(const std::string& randomstring,
                            BackendConnectionManagerPtr cm)
            : cm_(cm)
            , ns_(randomstring.empty() ? youtils::UUID().str() : randomstring)
        {
            LOG_INFO("Creating Random Namespace " << ns_);
            cm_->getConnection()->createNamespace(ns_);
        }

        ~WithRandomNamespace()
        {
            LOG_INFO("Deleting Random Namespace " << ns_);
            try
            {
                cm_->getConnection()->deleteNamespace(ns_);
            }
            catch(std::exception& e)
            {
                LOG_ERROR("Could not delete namespace " << ns_
                          << ": " << e.what()
                          << ", ignoring");
            }
            catch(...)
            {
                LOG_ERROR("Could not delete namespace " << ns_ << ", ignoring");
            }
        }

        const Namespace&
        ns() const
        {
            return ns_;
        }
        DECLARE_LOGGER("WithRandomNamespace");

    private:
        BackendConnectionManagerPtr cm_;
        Namespace ns_;
    };

    std::unique_ptr<WithRandomNamespace>
    make_random_namespace(const std::string& random_string = std::string(""))
    {
        return std::unique_ptr<WithRandomNamespace>(new WithRandomNamespace(random_string,
                                                                            cm_));
    }

    std::unique_ptr<WithRandomNamespace>
    make_random_namespace(const Namespace& ns)
    {
        return std::unique_ptr<WithRandomNamespace>(new WithRandomNamespace(ns.str(),
                                                                            cm_));
    }

    backend::BackendInterfacePtr
    createBackendInterface(const Namespace& nspace);

    void
    backendRegex(const Namespace& nspace,
                 const std::string& pattern,
                 std::list<std::string>& res);

    bool
    streamingSupport();

    static const BackendConfig&
    backend_config();

    void
    deleteNamespace(const Namespace& nspace);

    void
    createNamespace(const Namespace& nspace);

    void
    clearNamespace(const Namespace& nspace);

protected:
    backend::BackendConnectionManagerPtr cm_;

    void
    initialize_connection_manager();

    void
    uninitialize_connection_manager();

    static std::shared_ptr<ConnectionPool>
    connection_manager_pool(BackendConnectionManager&,
                            const Namespace&);

private:
    DECLARE_LOGGER("BackendTestSetup");
};

}

#endif //!BACKEND_TEST_SETUP_H_

// Local Variables: **
// mode: c++ **
// End: **
