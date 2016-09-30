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

#ifndef BACKEND_INTERFACE_H_
#define BACKEND_INTERFACE_H_

#include "BackendConnectionManager.h"
#include "BackendPolicyConfig.h"
#include "Condition.h"

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
class BackendRequestParameters;

// not a interface in terms of polymorphism anymore - rename?
class BackendInterface
{
    friend class BackendConnectionManager;

public:
    BackendInterface(const BackendInterface&) = delete;

    BackendInterface&
    operator=(BackendInterface&) = delete;

    static const BackendRequestParameters&
    default_request_parameters();

    void
    read(const boost::filesystem::path& dst,
         const std::string& name,
         InsistOnLatestVersion insist_on_latest,
         const BackendRequestParameters& = default_request_parameters());

    bool
    unique_tag_support() const;

    std::unique_ptr<youtils::UniqueObjectTag>
    get_tag(const std::string&,
            const BackendRequestParameters& = default_request_parameters());

    void
    write(const boost::filesystem::path& src,
          const std::string& name,
          const OverwriteObject = OverwriteObject::F,
          const youtils::CheckSum* chksum = 0,
          const boost::shared_ptr<Condition>& = nullptr,
          const BackendRequestParameters& = default_request_parameters());

    std::unique_ptr<youtils::UniqueObjectTag>
    write_tag(const boost::filesystem::path&,
              const std::string&,
              const youtils::UniqueObjectTag* prev_tag,
              const OverwriteObject = OverwriteObject::T,
              const BackendRequestParameters& = default_request_parameters());

    std::unique_ptr<youtils::UniqueObjectTag>
    read_tag(const boost::filesystem::path&,
             const std::string&,
             const BackendRequestParameters& = default_request_parameters());

    youtils::CheckSum
    getCheckSum(const std::string& name,
                const BackendRequestParameters& = default_request_parameters());

    bool
    namespaceExists(const BackendRequestParameters& = default_request_parameters());

    void
    createNamespace(const NamespaceMustNotExist = NamespaceMustNotExist::T,
                    const BackendRequestParameters& = default_request_parameters());

    void
    partial_read(const BackendConnectionInterface::PartialReads& partial_reads,
                 BackendConnectionInterface::PartialReadFallbackFun& fallback_fun,
                 InsistOnLatestVersion,
                 const BackendRequestParameters& = default_request_parameters());

    void
    deleteNamespace(const BackendRequestParameters& = default_request_parameters());

    void
    clearNamespace(const BackendRequestParameters& = default_request_parameters());

    void
    invalidate_cache(const BackendRequestParameters& = default_request_parameters());

    void
    invalidate_cache_for_all_namespaces(const BackendRequestParameters& = default_request_parameters());

    bool
    objectExists(const std::string& name,
                 const BackendRequestParameters& = default_request_parameters());

    void
    remove(const std::string& name,
           const ObjectMayNotExist = ObjectMayNotExist::F,
           const boost::shared_ptr<Condition>& = nullptr,
           const BackendRequestParameters& = default_request_parameters());

    BackendInterfacePtr
    clone() const;

    BackendInterfacePtr
    cloneWithNewNamespace(const Namespace&) const;

    uint64_t
    getSize(const std::string& name,
            const BackendRequestParameters& = default_request_parameters());

    void
    listObjects(std::list<std::string>& out,
                const BackendRequestParameters& = default_request_parameters());

    const Namespace&
    getNS() const;

    template<typename ObjectType,
             typename iarchive_type = typename ObjectType::iarchive_type>
    void
    fillObject(ObjectType& obj,
               const std::string& nameOnBackend,
               InsistOnLatestVersion insist_on_latest,
               const BackendRequestParameters& params = default_request_parameters())
    {
        try
        {
            const boost::filesystem::path
                p(youtils::FileUtils::create_temp_file_in_temp_dir(getNS().str() +
                                                                   "-" +
                                                                   nameOnBackend));
            ALWAYS_CLEANUP_FILE_TPL(p);
            read(p,
                 nameOnBackend,
                 insist_on_latest,
                 params);
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
               InsistOnLatestVersion insist_on_latest,
               const BackendRequestParameters& params = default_request_parameters())
    {
        try
        {
            const boost::filesystem::path
                p(youtils::FileUtils::create_temp_file_in_temp_dir(getNS().str() +
                                                                   "-" +
                                                                   nameOnBackend));
            ALWAYS_CLEANUP_FILE_TPL(p);
            read(p,
                 nameOnBackend,
                 insist_on_latest,
                 params);
            boost::filesystem::ifstream ifs(p);
            iarchive_type ia(ifs);
            ia & boost::serialization::make_nvp(nvp_name, obj);

            ifs.close();
        }
        CATCH_STD_ALL_LOG_RETHROW("Problem getting " << nameOnBackend << " from " <<
                                  getNS())
            }

    template<typename ObjectType>
    void
    fillObject(ObjectType& obj,
               InsistOnLatestVersion insist_on_latest,
               const BackendRequestParameters& params = default_request_parameters())
    {
        fillObject(obj,
                   ObjectType::config_backend_name,
                   insist_on_latest,
                   params);
    }

    template<typename ObjectType,
             typename oarchive_t = typename ObjectType::oarchive_type>
    void
    writeObject(const ObjectType& obj,
                const std::string& nameOnBackend,
                const OverwriteObject overwrite = OverwriteObject::F,
                const boost::shared_ptr<Condition>& cond = nullptr,
                const BackendRequestParameters& params = default_request_parameters())
    {
        try
        {
            const boost::filesystem::path
                p(youtils::FileUtils::create_temp_file_in_temp_dir(getNS().str() + "-" +
                                                                   nameOnBackend));
            ALWAYS_CLEANUP_FILE_TPL(p);
            youtils::Serialization::serializeAndFlush<oarchive_t, ObjectType>(p, obj);
            write(p,
                  nameOnBackend,
                  overwrite,
                  nullptr,
                  cond,
                  params);
        }
        CATCH_STD_ALL_LOG_RETHROW("Problem writing " << nameOnBackend << " to " <<
                                  getNS());
    }

    template<typename ObjectType,
             typename oarchive_t = typename ObjectType::oarchive_type>
    void
    writeObject(const ObjectType& obj,
                const OverwriteObject overwrite = OverwriteObject::F,
                const boost::shared_ptr<Condition>& cond = nullptr,
                const BackendRequestParameters& params = default_request_parameters())
    {
        return writeObject(obj,
                           ObjectType::config_backend_name,
                           overwrite,
                           cond,
                           params);
    }

    template<typename ObjectType>
    std::unique_ptr<ObjectType>
    getIstreamableObject(const std::string& nameOnBackend,
                         InsistOnLatestVersion insist_on_latest,
                         const BackendRequestParameters& params = default_request_parameters())
    {
        try
        {
            const boost::filesystem::path
                p(youtils::FileUtils::create_temp_file_in_temp_dir(getNS().str() + "-" +
                                                                   nameOnBackend));
            ALWAYS_CLEANUP_FILE_TPL(p);
            read(p,
                 nameOnBackend,
                 insist_on_latest,
                 params);
            boost::filesystem::ifstream ifs(p);
            return std::unique_ptr<ObjectType>(new ObjectType(ifs));
        }
        CATCH_STD_ALL_LOG_RETHROW("Problem getting " << nameOnBackend << " from " <<
                                  getNS());
    }

private:
    DECLARE_LOGGER("BackendInterface");

    const Namespace nspace_;
    BackendConnectionManagerPtr conn_manager_;

    // only the BackendConnectionManager is allowed to create it - everyone else needs to
    // use BackendInterfacePtrs obtained from the BackendConnectionManager.
    BackendInterface(const Namespace&,
                     BackendConnectionManagerPtr);

    template<typename ReturnType,
             typename... Args>
    ReturnType
    wrap_(const BackendRequestParameters&,
          ReturnType(BackendConnectionInterface::*mem_fun)(const Namespace&,
                                                           Args...),
          Args... args);

    template<typename ReturnType,
             typename... Args>
    ReturnType
    do_wrap_(const BackendRequestParameters&,
             ReturnType(BackendConnectionInterface::*mem_fun)(Args...),
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
