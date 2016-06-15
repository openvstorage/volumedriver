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

#ifndef ARAKOON_METADATA_DB_H_
#define ARAKOON_METADATA_DB_H_

#include "CachedMetaDataPage.h"
#include "ClusterLocationAndHash.h"
#include "MetaDataStoreInterface.h"
#include "MetaDataBackendInterface.h"

#include <set>
#include <vector>

#include <youtils/ArakoonInterface.h>
#include <youtils/Logging.h>
#include <youtils/IOException.h>

#include <backend/Namespace.h>

namespace volumedriver
{

class ArakoonMetaDataBackendConfig;
class VolumeConfig;

class ArakoonMetaDataBackend
    : public MetaDataBackendInterface
{
public:
    ArakoonMetaDataBackend(const ArakoonMetaDataBackendConfig& cfg,
                           const backend::Namespace& nspace);

    virtual ~ArakoonMetaDataBackend();

    bool
    getPage(CachePage& p) override final;

    inline bool
    getPage(CachePage& p,
            bool try_parent);

    void
    putPage(const CachePage& p,
            int32_t used_clusters_delta) override final;

    void
    discardPage(const CachePage& p,
                int32_t used_clusters_delta) override final;

    bool
    pageExistsInParent(const PageAddress pa) const override final;

    void
    sync() override final;


    // Make the database empty again
    void
    clear_all_keys() override final;

    MetaDataStoreFunctor&
    for_each(MetaDataStoreFunctor& f,
             uint64_t maxPages) override final;

    void
    setCork(const youtils::UUID& cork_uuid) override final;

    boost::optional<youtils::UUID>
    lastCorkUUID() override final;

    inline uint64_t
    getUsedClusters() const override final
    {
        return used_clusters_;
    }

    bool
    freezeable() const override final
    {
        return true;
    }

    bool
    hasFrozenParent() const override final
    {
        return static_cast<bool>(parent_ns_);
    }

    youtils::UUID
    setFrozenParentCloneCork() override final;

    bool
    isEmancipated() const override final
    {
        return not parent_cluster_;
    }

    std::unique_ptr<MetaDataBackendConfig>
    getConfig() const override final;

    virtual MaybeScrubId
    scrub_id() override final;

    virtual void
    set_scrub_id(const ScrubId& id) override final;

    static uint64_t
    locally_used_bytes(const VolumeConfig&)
    {
        return 0;
    }

    static uint64_t
    locally_required_bytes(const VolumeConfig&)
    {
        return 0;
    }

private:
    DECLARE_LOGGER("ArakoonMetaDataBackend");

    static const std::string separator_;
    static const std::string volume_prefix_;
    static const std::string cork_suffix_;
    static const std::string scrub_id_suffix_;
    static const std::string used_clusters_suffix_;
    static const std::string emancipated_suffix_;
    static const std::string page_dir_name_;
    static const uint32_t timeout_ms_;

    const backend::Namespace ns_;
    arakoon::Cluster cluster_;

    //    const std::string prefix_;
    const boost::optional<backend::Namespace> parent_ns_;
    const std::string page_dir_;
    const std::string parent_page_dir_;

    std::unique_ptr<arakoon::Cluster> parent_cluster_;

    std::set<PageAddress> parent_keys;

    const std::string cork_key_;
    const std::string scrub_id_key_;
    const std::string used_clusters_key_;
    const std::string emancipated_key_;

    uint64_t used_clusters_;

    // This is a bandaid as the arakoon::sequence cannot be inspected.
    std::vector<ClusterLocationAndHash> write_sequence_data_;
    std::vector<CachePage> write_sequence_;

    const ArakoonMetaDataBackendConfig config_;
    // virtual void
    // debugPrint();

    std::string
    corkUUIDName(const backend::Namespace& ns)
    {
	return get_key(cork_suffix_,
                       ns);
    }

    static inline std::string
    get_key(const std::string& name,
            const backend::Namespace& ns)
    {
        //        ASSERT(not ns.empty()); -> namespace cannot be empty
        ASSERT(name != "pages");
        return volume_prefix_ + ns.str() + separator_ + name;
    }

    PageAddress
    get_pageaddress_from_key(const arakoon::arakoon_buffer& buf,
                             const std::string& page_dir)
    {
        VERIFY(buf.first == page_dir.length() + sizeof(PageAddress));
        VERIFY(page_dir == std::string(reinterpret_cast<const char*>(buf.second), page_dir.length()));
        return *reinterpret_cast<const PageAddress*>(reinterpret_cast<const unsigned char*>(buf.second) + page_dir.length());
    }

    arakoon::sequence
    build_write_sequence_();

    void
    flush_write_sequence_(arakoon::sequence& seq, bool sync);

    void
    flush_write_sequence_(bool sync);

    void
    maybe_flush_write_sequence_();

    bool
    get_page_from_write_sequence_(CachePage& p);
};

}

#endif // !ARAKOON_METADATA_DB_H_

// Local Variables: **
// mode: c++ **
// End: **
