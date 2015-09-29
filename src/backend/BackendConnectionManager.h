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

#ifndef BACKEND_CONNECTION_MANAGER_H_
#define BACKEND_CONNECTION_MANAGER_H_

#include "BackendParameters.h"
#include "BackendConnectionInterface.h"
#include "Namespace.h"

#include <boost/ptr_container/ptr_list.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>

#include <youtils/BooleanEnum.h>
#include <youtils/ConfigurationReport.h>
#include <youtils/IOException.h>
#include <youtils/StrongTypedString.h>
#include <youtils/VolumeDriverComponent.h>

BOOLEAN_ENUM(ForceNewConnection)

namespace youtils
{
class UpdateReport;
}

namespace toolcut
{

class BackendToolCut;
class BackendConnectionToolCut;

}

namespace backend
{

class BackendConfig;
class BackendInterface;
typedef std::unique_ptr<BackendInterface> BackendInterfacePtr;

class BackendSinkInterface;
class BackendSourceInterface;

class BackendConnectionManager;
typedef std::shared_ptr<BackendConnectionManager> BackendConnectionManagerPtr;

class BackendConnectionDeleter
{
public:
    explicit BackendConnectionDeleter(BackendConnectionManagerPtr cm)
        : cm_(cm)
    {}

    ~BackendConnectionDeleter() = default;

    BackendConnectionDeleter(const BackendConnectionDeleter&) = default;

    BackendConnectionDeleter(BackendConnectionDeleter&&) = default;

    BackendConnectionDeleter&
    operator=(const BackendConnectionDeleter&) = default;

    BackendConnectionDeleter&
    operator=(BackendConnectionDeleter&&) = default;

    void
    operator()(BackendConnectionInterface* conn);

private:
    BackendConnectionManagerPtr cm_;
};

class BackendConnectionManager
    : public youtils::VolumeDriverComponent
    , public std::enable_shared_from_this<BackendConnectionManager>
{
public:
    // enfore usage via BackendConnectionManagerPtr
    // rationale: BackendConnPtrs hold a reference (via BackendConnectionManagerPtr),
    // so the BackendConnectionManager must not go out of scope before the last
    // reference is returned.
    static BackendConnectionManagerPtr
    create(const boost::property_tree::ptree& pt,
           const RegisterComponent registrate = RegisterComponent::T)
    {
        return BackendConnectionManagerPtr(new BackendConnectionManager(pt,
                                                                        registrate));
    }

    ~BackendConnectionManager();

    BackendConnectionManager(const BackendConnectionManager&) = delete;

    BackendConnectionManager&
    operator=(const BackendConnectionManager&) = delete;

#define LOCK_CONNS()                            \
    ::boost::lock_guard<lock_type> g__(lock_);

    inline BackendConnectionInterfacePtr
    getConnection(ForceNewConnection force_new = ForceNewConnection::F)
    {
        LOCK_CONNS();

        BackendConnectionDeleter d(shared_from_this());

        if (force_new == ForceNewConnection::T or connections_.empty())
        {
            BackendConnectionInterfacePtr conn(newConnection_(),
                                std::move(d));
            LOG_TRACE("handing out newly created connection handle " << conn.get());
            return conn;
        }
        else
        {
            BackendConnectionInterfacePtr conn(connections_.pop_front().release(),
                                std::move(d));
            conn->timeout(default_timeout);
            LOG_TRACE("handing out existing connection handle " << conn.get());
            return conn;
        }
    }

    BackendInterfacePtr
    newBackendInterface(const Namespace& nspace,
                        const unsigned retries = 1);

    const BackendConfig&
    config() const
    {
        return *config_;
    }

    std::unique_ptr<BackendSinkInterface>
    newBackendSink(const Namespace& nspace,
                   const std::string& name);

    std::unique_ptr<std::ostream>
    getOutputStream(const Namespace& nspace,
                    const std::string& name,
                    size_t buf_size = 4096);

    std::unique_ptr<BackendSourceInterface>
    newBackendSource(const Namespace& nspace,
                     const std::string& name);

    std::unique_ptr<std::istream>
    getInputStream(const Namespace& nspace,
                   const std::string& name,
                   size_t buf_size = 4096);

    size_t
    capacity() const;

    size_t
    size() const;

    // VolumeDriverComponent Interface
    virtual void
    persist(boost::property_tree::ptree& pt,
            const ReportDefault reportDefault = ReportDefault::F) const override final;

    virtual const char*
    componentName() const override final;

    virtual void
    update(const boost::property_tree::ptree& pt,
           youtils::UpdateReport& report) override final;

    virtual bool
    checkConfig(const boost::property_tree::ptree& pt,
                youtils::ConfigurationReport& rep) const override final;
    // end VolumeDriverComponent Interface

private:
    DECLARE_LOGGER("BackendConnectionManager");

    DECLARE_PARAMETER(backend_connection_pool_capacity);
    std::unique_ptr<BackendConfig> config_;

    typedef boost::mutex lock_type;
    mutable lock_type lock_;

    boost::ptr_list<BackendConnectionInterface> connections_;

    // Create through
    // static BackendConnectionManagerPtr
    // create(const boost::property_tree::ptree& pt,
    //        const RegisterComponent registrate = RegisterComponent::T)
    explicit BackendConnectionManager(const boost::property_tree::ptree&,
                                      const RegisterComponent reg = RegisterComponent::T);

    BackendConnectionInterface*
    newConnection_();

    boost::posix_time::time_duration default_timeout;

    void
    releaseConnection(BackendConnectionInterface* conn);

#undef LOCK_CONNS

    friend class BackendConnectionDeleter;
    friend class toolcut::BackendToolCut;
    friend class toolcut::BackendConnectionToolCut;
};

}

#endif // !BACKEND_CONNECTION_MANAGER_H_

// Local Variables: **
// mode: c++ **
// End: **
