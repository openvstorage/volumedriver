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

#ifndef MULTI_CONNECTION_H
#define MULTI_CONNECTION_H

#include "BackendException.h"
#include "BackendConnectionInterface.h"
#include "MultiConfig.h"

#include <boost/filesystem.hpp>

#include <youtils/IOException.h>
#include <youtils/Logging.h>
#include <youtils/wall_timer.h>

namespace backend
{
namespace multi
{
namespace fs = boost::filesystem;

class Connection
    : public BackendConnectionInterface
{
public:
    typedef MultiConfig config_type;

    explicit Connection(const config_type& cfg);

    virtual ~Connection();

    Connection(const Connection&) = delete;

    Connection&
    operator=(const Connection&) = delete;

    virtual void
    listNamespaces_(std::list<std::string>& objects) override final;

    virtual void
    listObjects_(const Namespace& nspace,
                 std::list<std::string>& objects) override final;

    virtual bool
    partial_read_(const Namespace&,
                  const PartialReads&,
                  InsistOnLatestVersion) override final;

    virtual bool
    namespaceExists_(const Namespace& nspace) override final;

    virtual void
    createNamespace_(const Namespace& nspace,
                     const NamespaceMustNotExist = NamespaceMustNotExist::T) override final;

    virtual void
    deleteNamespace_(const Namespace& nspace) override final;

    DECLARE_LOGGER("MultiConnection");

    // void
    // print(std::ostream&) const;

    virtual bool
    healthy() const override final
    {
        return true;
    }

    virtual void
    read_(const Namespace& nspace,
          const fs::path& Destination,
          const std::string& Name,
          const InsistOnLatestVersion insist_on_latest) override final;

    virtual std::unique_ptr<youtils::UniqueObjectTag>
    get_tag_(const Namespace&,
             const std::string&) override final;

    virtual void
    write_(const Namespace&,
           const fs::path&,
           const std::string&,
           const OverwriteObject = OverwriteObject::F,
           const youtils::CheckSum* = nullptr,
           const boost::shared_ptr<Condition>& = nullptr) override final;

    virtual std::unique_ptr<youtils::UniqueObjectTag>
    write_tag_(const Namespace&,
               const boost::filesystem::path&,
               const std::string&,
               const youtils::UniqueObjectTag*,
               const OverwriteObject) override final;

    virtual std::unique_ptr<youtils::UniqueObjectTag>
    read_tag_(const Namespace&,
              const boost::filesystem::path&,
              const std::string&) override final;

    virtual bool
    objectExists_(const Namespace& nspace,
                  const std::string& name) override final;

    virtual bool
    hasExtendedApi_() const override final;


    virtual void
    remove_(const Namespace&,
            const std::string&,
            const ObjectMayNotExist,
            const boost::shared_ptr<Condition>& = nullptr) override final;

    virtual uint64_t
    getSize_(const Namespace& nspace,
             const std::string& name) override final;

    virtual youtils::CheckSum
    getCheckSum_(const Namespace& nspace,
                 const std::string& name) override final;

    virtual backend::ObjectInfo
    x_getMetadata_(const Namespace& nspace,
                   const std::string& name) override final;

    virtual backend::ObjectInfo
    x_setMetadata_(const Namespace& nspace,
                   const std::string& name,
                   const backend::ObjectInfo::CustomMetaData& metadata) override final;

    virtual backend::ObjectInfo
    x_updateMetadata_(const Namespace& nspace,
                      const std::string& name,
                      const backend::ObjectInfo::CustomMetaData& metadata) override final;

    //the x_read_* functions can also return ObjectInfo but that's more involved as it's not returned as Json

    virtual backend::ObjectInfo
    x_read_(const Namespace& nspace,
            const fs::path& destination,
            const std::string& name,
            const InsistOnLatestVersion) override final;

    virtual backend::ObjectInfo
    x_read_(const Namespace& nspace,
            std::string& destination,
            const std::string& name,
            const InsistOnLatestVersion) override final;

    virtual backend::ObjectInfo
    x_read_(const Namespace& nspace,
            std::stringstream& destination,
            const std::string& name,
            const InsistOnLatestVersion) override final;

    virtual backend::ObjectInfo
    x_write_(const Namespace& nspace,
             const fs::path &location,
             const std::string& name,
             const OverwriteObject,
             const backend::ETag* etag,
             const youtils::CheckSum* chksum) override final;

    virtual backend::ObjectInfo
    x_write_(const Namespace& nspace,
             const std::string& istr,
             const std::string& name,
             const OverwriteObject,
             const backend::ETag* etag,
             const youtils::CheckSum* chksum) override final;

    virtual backend::ObjectInfo
    x_write_(const Namespace& nspace,
             std::stringstream& strm,
             const std::string& name,
             const OverwriteObject,
             const backend::ETag* etag,
             const youtils::CheckSum* chksum) override final;

private:
    typedef std::vector<BackendConnectionInterface*>::iterator iterator_t;
    std::vector<BackendConnectionInterface*> backends_;
    std::vector<BackendConnectionInterface*>::iterator current_iterator_;
    youtils::wall_timer2 switch_back_timer_;
    const unsigned switch_back_seconds = 10 * 60;

    bool
    update_current_index(const iterator_t& start_iterator)
    {
        switch_back_timer_.restart();

        LOG_WARN("Trying to switch to a new backend");
        if(++current_iterator_ == backends_.end() )
        {
            current_iterator_ = backends_.begin();
        }

        return current_iterator_ == start_iterator;
    }

    bool
    maybe_switch_back_to_default()
    {
        if(current_iterator_ != backends_.begin())
        {
            if(switch_back_timer_.elapsed_in_seconds() > switch_back_seconds)
            {
                current_iterator_ = backends_.begin();
            }
        }
        return current_iterator_ != backends_.end();
    }

};
}
}



#endif // MULTI_CONNECTION_H

// Local Variables: **
// mode: c++ **
// End: **
