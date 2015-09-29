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

#ifndef BACKEND_INTERFACE_H_
#define BACKEND_INTERFACE_H_

#include "BackendConnectionManager.h"
#include "BackendPolicyConfig.h"

#include <functional>

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <youtils/Catchers.h>
#include <youtils/CheckSum.h>
#include <youtils/FileUtils.h>
#include <youtils/Logging.h>
#include <youtils/Serialization.h>

namespace backend
{

class BackendConnectionInterface;

// not a interface in terms of polymorphism anymore - rename?
class BackendInterface
{
    friend class BackendConnectionManager;

public:
    BackendInterface(const BackendInterface&) = delete;

    BackendInterface&
    operator=(BackendInterface&) = delete;

    void
    read(const boost::filesystem::path& dst,
         const std::string& name,
         InsistOnLatestVersion insist_on_latest);

    void
    write(const boost::filesystem::path& src,
          const std::string& name,
          const OverwriteObject = OverwriteObject::F,
          const youtils::CheckSum* chksum = 0);

    youtils::CheckSum
    getCheckSum(const std::string& name);

    bool
    namespaceExists();

    void
    createNamespace();

    void
    partial_read(const BackendConnectionInterface::PartialReads& partial_reads,
                 BackendConnectionInterface::PartialReadFallbackFun& fallback_fun,
                 InsistOnLatestVersion);

    void
    deleteNamespace();

    void
    clearNamespace();

    void
    invalidate_cache();

    void
    invalidate_cache_for_all_namespaces();

    bool
    objectExists(const std::string& name);

    void
    remove(const std::string& name,
           const ObjectMayNotExist = ObjectMayNotExist::F);

    BackendInterfacePtr
    clone() const;

    BackendInterfacePtr
    cloneWithNewNamespace(const Namespace& new_nspace) const;

    uint64_t
    getSize(const std::string& name);

    void
    listObjects(std::list<std::string>& out);

    const Namespace&
    getNS() const;

    template<typename ObjectType,
             typename iarchive_type = typename ObjectType::iarchive_type>
    void
    fillObject(ObjectType& obj,
               const std::string& nameOnBackend,
               InsistOnLatestVersion insist_on_latest)
    {
        try
        {
            const boost::filesystem::path
                p(youtils::FileUtils::create_temp_file_in_temp_dir(getNS().str() +
                                                                   "-" +
                                                                   nameOnBackend));
            ALWAYS_CLEANUP_FILE_TPL(p);
            read(p, nameOnBackend, insist_on_latest);
            boost::filesystem::ifstream ifs(p);
            iarchive_type ia(ifs);
            ia & obj;
            ifs.close();
        }
        CATCH_STD_ALL_LOG_RETHROW("Problem getting " << nameOnBackend << " from " <<
                                  getNS());
    }

    template<typename ObjectType,
             typename iarchive_type = typename ObjectType::iarchive_type>
    void
    fillObject(ObjectType& obj,
               const std::string& nameOnBackend,
               const char* nvp_name,
               InsistOnLatestVersion insist_on_latest)
    {
        try
        {
            const boost::filesystem::path
                p(youtils::FileUtils::create_temp_file_in_temp_dir(getNS().str() +
                                                                   "-" +
                                                                   nameOnBackend));
            ALWAYS_CLEANUP_FILE_TPL(p);
            read(p, nameOnBackend, insist_on_latest);
            boost::filesystem::ifstream ifs(p);
            iarchive_type ia(ifs);
            ia & boost::serialization::make_nvp(nvp_name, obj);

            ifs.close();
        }
        CATCH_STD_ALL_LOG_RETHROW("Problem getting " << nameOnBackend << " from " <<
                                  getNS())
            }

    bool
    hasExtendedApi();

    // Add the other x_{read,write} flavours as needed.
    ObjectInfo
    x_read(std::string& destination,
           const std::string& name,
           InsistOnLatestVersion insist_on_latest);

    ObjectInfo
    x_read(std::stringstream& dst,
           const std::string& name,
           InsistOnLatestVersion insist_on_latest);

    ObjectInfo
    x_write(const std::string& istr,
            const std::string& name,
            const OverwriteObject overwrite = OverwriteObject::F,
            const backend::ETag* etag = 0,
            const youtils::CheckSum* chksum = 0);

    template<typename ObjectType>
    void
    fillObject(ObjectType& obj,
               InsistOnLatestVersion insist_on_latest)
    {
        fillObject(obj, ObjectType::config_backend_name, insist_on_latest);
    }

    template<typename ObjectType,
             typename oarchive_t = typename ObjectType::oarchive_type>
    void
    writeObject(const ObjectType& obj,
                const std::string& nameOnBackend,
                const OverwriteObject overwrite = OverwriteObject::F)
    {
        try
        {
            const boost::filesystem::path
                p(youtils::FileUtils::create_temp_file_in_temp_dir(getNS().str() + "-" +
                                                                   nameOnBackend));
            ALWAYS_CLEANUP_FILE_TPL(p);
            youtils::Serialization::serializeAndFlush<oarchive_t, ObjectType>(p, obj);
            write(p, nameOnBackend, overwrite);
        }
        CATCH_STD_ALL_LOG_RETHROW("Problem writing " << nameOnBackend << " to " <<
                                  getNS());
    }

    template<typename ObjectType,
             typename oarchive_t = typename ObjectType::oarchive_type>
    void
    writeObject(const ObjectType& obj,
                const OverwriteObject overwrite = OverwriteObject::F)
    {
        return writeObject(obj,
                           ObjectType::config_backend_name,
                           overwrite);
    }

    template<typename ObjectType>
    std::unique_ptr<ObjectType>
    getIstreamableObject(const std::string& nameOnBackend,
                         InsistOnLatestVersion insist_on_latest)
    {
        try
        {
            const boost::filesystem::path
                p(youtils::FileUtils::create_temp_file_in_temp_dir(getNS().str() + "-" +
                                                                   nameOnBackend));
            ALWAYS_CLEANUP_FILE_TPL(p);
            read(p, nameOnBackend, insist_on_latest);
            boost::filesystem::ifstream ifs(p);
            return std::unique_ptr<ObjectType>(new ObjectType(ifs));
        }
        CATCH_STD_ALL_LOG_RETHROW("Problem getting " << nameOnBackend << " from " <<
                                  getNS())
            }

private:
    DECLARE_LOGGER("BackendInterface");

    const Namespace nspace_;
    BackendConnectionManagerPtr conn_manager_;
    const boost::posix_time::time_duration timeout_;
    const unsigned retries_;

    // only the BackendConnectionManager is allowed to create it - everyone else needs to
    // use BackendInterfacePtrs obtained from the BackendConnectionManager.
    BackendInterface(const Namespace& nspace,
                     BackendConnectionManagerPtr cm,
                     boost::posix_time::time_duration timeout,
                     unsigned retries = 1);

    template<typename ReturnType,
             typename... Args>
    ReturnType
    wrap_(ReturnType(BackendConnectionInterface::*mem_fun)(const Namespace&,
                                                           Args...),
          Args... args);

    template<typename ReturnType,
             typename... Args>
    ReturnType
    do_wrap_(ReturnType(BackendConnectionInterface::*mem_fun)(Args...),
             Args... args);

    template<typename R>
    R
    handle_eventual_consistency_(InsistOnLatestVersion insist_on_latest,
                                 std::function<R(InsistOnLatestVersion)>&& fun);
};

}

#endif // !BACKEND_INTERFACE_H_

// Local Variables: **
// mode: c++ **
// End: **
