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

#ifndef BACKEND_WRAPPER_H_
#define BACKEND_WRAPPER_H_

#include "BackendConnectionInterface.h"
#include "BackendParameters.h"

#include <boost/property_tree/ptree_fwd.hpp>

namespace backendproto
{

enum class BackendType
{
    Rest,
    S3
};

class BackendInterface
{
public:
    BackendInterface(BackendType type)
        : type_(type)
    {}

    BackendInterface(const BackendInterface& bi)
        : type_(bi.type_)
    {}

    virtual ~BackendInterface()
    {}

    virtual void
    get() = 0;

    virtual void
    set() = 0;

    const BackendType type_;
};

template <BackendType t>
class Backend;

template<>
class Backend<BackendType::S3>
    : public BackendInterface
{
public:
    Backend<BackendType::S3>(const boost::property_tree::ptree& pt)
    : BackendInterface(BackendType::S3)
        , s3_connection_host(pt)
        , s3_connection_port(pt)
        , s3_connection_username(pt)
        , s3_connection_password(pt)
        , s3_connection_verbose_logging(pt)
    {}

    Backend<BackendType::S3>(const std::string& host,
                             uint16_t port,
                             const std::string& username,
                             const std::string& password,
                             const bool verbose_logging)

    : BackendInterface(BackendType::S3)
        , s3_connection_host(host)
        , s3_connection_port(port)
        , s3_connection_username(username)
        , s3_connection_password(password)
        , s3_connection_verbose_logging(verbose_logging)
    {}


    void
    get();

    void
    set();

    DECLARE_PARAMETER(s3_connection_host);
    DECLARE_PARAMETER(s3_connection_port);
    DECLARE_PARAMETER(s3_connection_username);
    DECLARE_PARAMETER(s3_connection_password);
    DECLARE_PARAMETER(s3_connection_verbose_logging);
};

BackendType
get_type(const boost::property_tree::ptree&)
{
    return BackendType::Rest;
}

std::unique_ptr<BackendInterface>
make_backend(const boost::property_tree::ptree& pt)
{
    BackendType type = get_type(pt);

    switch(type)
    {
    case BackendType::S3:
        return std::unique_ptr<BackendInterface>(new Backend<BackendType::S3>(pt));
    }
    UNREACHABLE;
}

class BackendConnectionManager
{

};

/*

template<typename T>
class WrapperClassConfig
{

    // Some values
    typename T::config_type t_config;
};

template<typename BackendType>
struct WrapperClass :: some_supertype
{

};

template<typename RealBackend,
         typename WrapperClass>
struct BackendWrapperConfig
{
    WrapperClass::config_type w;
    RealBackend::config_type t;
};

template<typename RealBackend,
         typename WrapperClass>
class BackendWrapper : public BackendConnectionInterface
{

    typedef BackendWrapperConfig<RealBackend,
                                 WrapperClass> config_type;


public:
    // input the args to make the wrapper
    BackendWrapper(const WrapperConfig& wrapper)
        : realBackend_(realBackend)
        , wrapper_(wrapper)
    {

    }

    virtual void
    createPolicy_(const BackendPolicyConfig& policy,
                  Guid& policy_id) = 0;

    virtual void
    listObjects_(const Namespace& ns,
                 std::list<std::string>& objects)
    {
        wrapperClass.listObjects(realBackend,
                                 ns,
                                 objects);
    }


    virtual bool
    namespaceExists_(const Namespace& ns)
    {
        wrapperClass.namespaceExists(realBackend,
                                     objects)
    }



    virtual void
    createNamespace_(const Namespace& ns,
                     const Guid& policy_id)
    {
        wrapperClass.createNamespace(realBackend,
                                     ns,
                                     policy_id);
    }


    virtual void
    deleteNamespace_(const Namespace& nspace)
    {
        wrapperClass.deleteNamespace(nspace);
    }


    virtual void
    read_(const Namespace& ns,
          const fs::path& destination,
          const std::string& Name)
    {
        wrapperClass.read(realBackend,
                          ns,
                          destination,
                          name);
    }


    virtual void
    write_(const Namespace& ns,
           const fs::path &location,
           const std::string& name,
           const OverwriteObject overwriteObject = OverwriteObject::F,
           const CheckSum* chksum = 0)
    {
        wrapperClass.write(realBackend,
                           ns,
                           location,
                           name,
                           overwriteObject,
                           chksum);
    }


    virtual bool
    objectExists_(const Namespace&,
                  const std::string& name)
    {
        wrapperClass.objectExists(realBackend,
                                  ns,
                                  name);
    }


    virtual void
    remove_(const Namespace&,
            const std::string& name,
            const ObjectMayNotExist) = 0;

    virtual uint64_t
    getSize_(const Namespace&,
             const std::string& name) = 0;

    virtual youtils::CheckSum
    getCheckSum_(const Namespace&,
                 const std::string& name) = 0;

    // Will return false on a non functioning extended interface
    virtual bool
    hasExtendedApi_() const = 0;

    // This is the Extended Api which is Only on Rest and partially on the local Backend
    virtual ObjectInfo
    x_getMetadata_(const Namespace&,
                   const std::string& name) = 0;

    virtual ObjectInfo
    x_setMetadata_(const Namespace&,
                   const std::string& name,
                   const ObjectInfo::CustomMetaData& metadata) = 0;

    virtual ObjectInfo
    x_updateMetadata_(const Namespace&,
                      const std::string& name,
                      const ObjectInfo::CustomMetaData& metadata) = 0;

    //the x_read_* functions can also return ObjectInfo but that's more involved as it's not returned as Json

    virtual ObjectInfo
    x_read_(const Namespace&,
            const fs::path& destination,
            const std::string& name) = 0;

    virtual ObjectInfo
    x_read_(const Namespace&,
            std::string& destination,
            const std::string& name) = 0;

    virtual ObjectInfo
    x_read_(const Namespace&,
            std::stringstream& destination,
            const std::string& name) = 0;

    virtual ObjectInfo
    x_write_(const Namespace&,
             const fs::path &location,
             const std::string& name,
             const OverwriteObject = OverwriteObject::F,
             const backend::ETag* etag = 0,
             const CheckSum* chksum = 0) = 0;

    virtual ObjectInfo
    x_write_(const Namespace&,
             const std::string& istr,
             const std::string& name,
             const OverwriteObject = OverwriteObject::F,
             const backend::ETag* etag = 0,
             const CheckSum* chksum = 0) = 0;

    virtual ObjectInfo
    x_write_(const Namespace&,
             std::stringstream& strm,
             const std::string& name,
             const OverwriteObject = OverwriteObject::F,
             const backend::ETag* etag = 0,
             const CheckSum* chksum = 0) = 0;

private:
    RealBackend realBackend_;
    WrapperClass wrapper_;
}
*/
}

#endif // BACKEND_WRAPPER_H_
