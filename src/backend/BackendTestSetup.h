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

#ifndef BACKEND_TEST_SETUP_H_
#define BACKEND_TEST_SETUP_H_

#include "BackendConnectionManager.h"
#include "BackendConfig.h"

#include <loki/Typelist.h>
#include <youtils/Logging.h>
#include <youtils/TestBase.h>
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



private:
    DECLARE_LOGGER("BackendTestSetup");

};

}

#endif //!BACKEND_TEST_SETUP_H_

// Local Variables: **
// mode: c++ **
// End: **
