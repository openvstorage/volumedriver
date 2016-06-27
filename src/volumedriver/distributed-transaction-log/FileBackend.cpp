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

#include "FileBackend.h"
#include "fungilib/WrapByteArray.h"

#include <boost/scoped_array.hpp>

#include <youtils/Assert.h>
#include <youtils/ScopeExit.h>

namespace distributed_transaction_log
{

namespace fs = boost::filesystem;
namespace vd = volumedriver;
namespace yt = youtils;

FileBackend::FileBackend(const fs::path& root,
                         const std::string& nspace,
                         const vd::ClusterSize cluster_size)
    : Backend(nspace,
                          cluster_size)
    , root_(root / nspace)
{
    LOG_INFO("creating " << root_);
    fs::create_directories(root_);
}

FileBackend::~FileBackend()
{
    LOG_INFO("removing " << root_);

    try
    {
        fs::remove_all(root_);
    }
    CATCH_STD_ALL_LOG_IGNORE(getNamespace() << ": failed to remove " << root_);
}

fs::path
FileBackend::make_path_(const vd::SCO sco) const
{
    return root_ / sco.str();
}

void
FileBackend::flush()
{
    if (file_)
    {
        file_->flush();
    }
}

void
FileBackend::remove(const vd::SCO sco)
{
    LOG_INFO(getNamespace() << ": removing " << sco);
    fs::remove(make_path_(sco));
}

void
FileBackend::close()
{
    file_ = nullptr;
}

void
FileBackend::open(const vd::SCO sco)
{
    const fs::path p(make_path_(sco));
    LOG_INFO(getNamespace() << ": opening " << p);

    file_ = std::make_unique<fungi::File>(p.string(),
                                          fungi::File::Append);

    file_->open();
}

void
FileBackend::add_entries(std::vector<vd::FailOverCacheEntry> entries,
                         std::unique_ptr<uint8_t[]> buf)
{
    VERIFY(file_);

    for (const auto& e : entries)
    {
        const vd::ClusterLocation& loc = e.cli_;

        VERIFY(loc.version() == 0);
        VERIFY(loc.cloneID() == 0);

        fungi::IOBaseStream os(*file_);
        // TODO: fix WrapByteArray or better yet get rid of it.
        os << loc << e.lba_ << fungi::WrapByteArray(const_cast<uint8_t*>(e.buffer_),
                                                    e.size_);
    }
}

void
FileBackend::get_entries(const vd::SCO sco,
                         Backend::EntryProcessorFun& fun)
{
    LOG_INFO(getNamespace() << ": processing SCO " << sco);

    const fs::path filename(make_path_(sco));
    VERIFY(fs::exists(filename));

    fungi::File f(filename.string(),
                  fungi::File::Read);
    f.open();
    fungi::IOBaseStream fstream(f);

    // TODO: IOBaseStream should do that itself. Get rid of it.
    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         fstream.close();
                                     }));

    boost::scoped_array<byte> buf(new byte[cluster_size()]);

    while (!f.eof())
    {
        vd::ClusterLocation cl;
        fstream >> cl;

        uint64_t lba;
        fstream >> lba;

        int64_t bal; // byte array length
        fstream >> bal;
        int32_t len = (int32_t) bal;
        VERIFY(len == static_cast<int32_t>(cluster_size()));
        fstream.read(buf.get(), len);

        LOG_DEBUG(getNamespace() << ": sending entry " << cl
                  << ", lba " << lba);

        fun(cl,
            lba,
            buf.get(),
            len);
    }
}

}
