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

#include "Local_Connection.h"

#include <boost/filesystem/fstream.hpp>

#include <youtils/Assert.h>
#include <youtils/CheckSum.h>
#include <youtils/FileUtils.h>
#include <youtils/FileDescriptor.h>
#include <youtils/ObjectMd5.h>

namespace backend
{

namespace local
{

namespace bi = boost::interprocess;
namespace fs = boost::filesystem;
namespace yt = youtils;

Connection::LRUCacheType&
Connection::lruCache()
{
    static LRUCacheType lru_cache("Local Backend Cache",
                                  Connection::lru_cache_size,
                                  Connection::EvictFromCache);
    return lru_cache;
}

namespace
{

DECLARE_LOGGER("LocalConnectionUtils");

// bi::named_mutex names have to abide by the rules set forth by sem_overview(7):
// "A named semaphore is identified by a name  of the form /somename; that is, a
// null-terminated string  of up to NAME_MAX-4 (i.e., 251) characters consisting
// of an initial slash, followed by one or more characters, none of which are
// slashes."
std::string
make_mutex_name(const fs::path& path)
{
    VERIFY(path.string().size() < NAME_MAX - 4);
    std::string s;
    bool fst = true;
    for (const auto& p : path)
    {
        if (fst)
        {
            fst = false;
        }
        else
        {
            s += "-";
        }

        s += p.string();
    }

    VERIFY(s[0] == '/');

    return s;
}

}

// ALERT:
// THIS LOCKING IS HERE TO MAKE THE TESTS WORK AND IS HIGHLY UNOPTIMIZED
#define LOCK_BACKEND()  \
    boost::lock_guard<decltype(lock_)> l_g__(lock_);

Connection::Connection(const config_type& cfg)
    : Connection(cfg.local_connection_path.value(),
                 timespec{cfg.local_connection_tv_sec.value(),
                         cfg.local_connection_tv_nsec.value()},
                 cfg.local_connection_enable_partial_read.value() ?
                 EnablePartialRead::T :
                 EnablePartialRead::F,
                 cfg.local_connection_sync_object_after_write.value() ?
                 SyncObjectAfterWrite::T :
                 SyncObjectAfterWrite::F)
{
    if(not fs::exists(path_))
    {
        LOG_ERROR("Backend root " << path_ << " does not exist");
        throw BackendBackendException();
    }
}

Connection::Connection(const fs::path& path,
                       const timespec& t,
                       const EnablePartialRead enable_partial_read,
                       const SyncObjectAfterWrite sync_object_after_write)
    : path_(path)
    , lock_(bi::open_or_create_t(),
            make_mutex_name(path_).c_str())
    , timespec_(t)
    , enable_partial_read_(enable_partial_read)
    , sync_object_after_write_(sync_object_after_write)
{
    LOG_TRACE("created new connection " << this);

    if(not fs::exists(path_))
    {
        LOG_ERROR("Backend root " << path_ << " does not exist");
        throw BackendBackendException();
    }
}

Connection::~Connection()
{
    bi::named_mutex::remove(make_mutex_name(path_).c_str());
}

bool
Connection::namespaceExists_(const Namespace& nspace)
{
    return fs::exists(nspacePath_(nspace));
}

yt::CheckSum
Connection::getCheckSum_(const Namespace& nspace,
                         const std::string& name)
{
    const fs::path src(checkedObjectPath_(nspace, name));
    return youtils::FileUtils::calculate_checksum(src);
}

void
Connection::createNamespace_(const Namespace& nspace,
                             const NamespaceMustNotExist must_not_exist)
{
    if (must_not_exist == NamespaceMustNotExist::T and
        fs::exists(nspacePath_(nspace)))
    {
        LOG_ERROR("Namespace " << nspace << " already exists");
        throw BackendCouldNotCreateNamespaceException();
    }

    fs::create_directories(nspacePath_(nspace));
}

void
Connection::read_(const Namespace& nspace,
                  const fs::path& dst,
                  const std::string& name,
                  InsistOnLatestVersion)
{
    nanosleep(&timespec_,0);

    if(name.empty())
    {
        LOG_ERROR("no name specified");
        throw BackendClientException();
    }

    const fs::path src(checkedObjectPath_(nspace, name));

    try
    {
        LOG_DEBUG("copying " << src << " -> " << dst);
        youtils::FileUtils::safe_copy(src,
                                      dst,
                                      youtils::SyncFileBeforeRename(sync_object_after_write_));
        LOG_DEBUG("copied " << src << " -> " << dst);
    }
    catch (youtils::FileUtils::CopyException& e)
    {
        // could also be a problem on the filesystem we try to restore to
        LOG_ERROR("Failed to copy " << src << " -> " << dst << ": " << e.what());
        throw BackendRestoreException();
    }
}

void
Connection::write_(const Namespace& nspace,
                   const fs::path &src,
                   const std::string& name,
                   const OverwriteObject overwrite,
                   const yt::CheckSum* chksum,
                   const boost::shared_ptr<Condition>& cond)
{
    nanosleep(&timespec_,0);

    LOG_DEBUG("src path " << src << " name " << name);

    if (chksum != 0)
    {
        yt::CheckSum calc(youtils::FileUtils::calculate_checksum(src));
        if (calc != *chksum)
        {
            LOG_FATAL(src << ": checksum mismatch: expected " << *chksum <<
                      ", calculated " << calc);

            throw BackendInputException();
        }
    }

    LOCK_BACKEND();

    copy_(nspace,
          src,
          name,
          overwrite,
          cond ? &cond->object_name() : nullptr,
          cond ? &cond->object_tag() : nullptr);
}

void
Connection::verify_tag_(const Namespace& nspace,
                        const std::string* tag_name,
                        const yt::UniqueObjectTag* prev_tag) const
{
    if (prev_tag)
    {
        VERIFY(tag_name);

        auto prev_md5 = dynamic_cast<const yt::ObjectMd5*>(prev_tag);
        VERIFY(prev_md5);

        const fs::path p(checkedObjectPath_(nspace,
                                            *tag_name));

        fs::ifstream ifs(p);
        const yt::Weed w(ifs);
        const yt::ObjectMd5 md5(w);
        if (md5 != *prev_md5)
        {
            LOG_ERROR(p << ": tag mismatch, have " << md5 <<
                      ", expected " << *prev_md5);
            throw BackendUniqueObjectTagMismatchException();
        }
    }
}

void
Connection::copy_(const Namespace& nspace,
                  const fs::path& src,
                  const std::string& name,
                  const OverwriteObject overwrite,
                  const std::string* tag_name,
                  const yt::UniqueObjectTag* prev_tag)
{
    verify_tag_(nspace,
                tag_name,
                prev_tag);

    auto dst = objectPath_(nspace, name);
    bool pre_exists = fs::exists(dst);

    if (pre_exists and F(overwrite))
    {
        LOG_ERROR("Target already in backend " << dst);
        throw BackendOverwriteNotAllowedException();
    }
    try
    {
        LOG_DEBUG("copying " << src << " -> " << dst);
        youtils::FileUtils::safe_copy(src,
                                      dst,
                                      youtils::SyncFileBeforeRename(sync_object_after_write_));
        if(pre_exists)
        {
            lruCache().erase_no_evict(dst);
        }

        LOG_DEBUG("copied " << src << " -> " << dst);
    }
    catch (youtils::FileUtils::CopyNoSourceException& e)
    {
        LOG_ERROR("Source does not exist " << src);
        throw BackendInputException();
    }
    catch (std::exception& e)
    {
        LOG_ERROR("Failed to copy " << src << " -> " << dst << ": " << e.what());
        throw BackendStoreException();
    }
}

bool
Connection::objectExists_(const Namespace& nspace,
                          const std::string& name)
{
    const fs::path dst(objectPath_(nspace, name));

    // LOCK_BACKEND();
    const bool b = fs::exists(dst);

    LOG_TRACE(dst << ": " << b);

    if (not b and not namespaceExists_(nspace))
    {
        throw BackendNamespaceDoesNotExistException();
    }

    return b;
}

void
Connection::remove_(const Namespace& nspace,
                    const std::string& name,
                    const ObjectMayNotExist may_not_exist,
                    const boost::shared_ptr<Condition>& cond)
{
    LOCK_BACKEND();

    const fs::path src(objectPath_(nspace, name));
    LOG_INFO("removing " << src);

    verify_tag_(nspace,
                cond ? &cond->object_name() : nullptr,
                cond ? &cond->object_tag() : nullptr);

    // TODO: failure is ignored for now
    if (fs::exists(src))
    {
        LOG_TRACE("removing " << src);
        fs::remove(src);
        lruCache().erase_no_evict(src);
    }
    else
    {
        if (not namespaceExists_(nspace))
        {
            throw BackendNamespaceDoesNotExistException();
        }

        if(F(may_not_exist))
        {
            LOG_TRACE("*not* removing " << src << " as it doesn't appear to exist");
            throw BackendObjectDoesNotExistException();
        }
    }
}

void
Connection::deleteNamespace_(const Namespace& nspace)
{
    LOG_INFO("deleting " << nspace);
    const fs::path pth = nspacePath_(nspace);
    if (not fs::exists(pth))
    {
        LOG_ERROR("Namespace " << nspace << " does not exist");
        throw BackendCouldNotDeleteNamespaceException();
    }

    const fs::path temp_dir = youtils::FileUtils::create_temp_dir(path_,
                                                                  ".temp");
    fs::rename(pth,
               temp_dir);

    for(const auto& key : lruCache().keys())
    {
        if(key.parent_path() == pth)
        {
            lruCache().erase_no_evict(key);
        }
    }

    try
    {
        fs::remove_all(temp_dir);
    }
    catch(std::exception& e)
    {
        LOG_ERROR("Exception trying to remove the tmp dir: " << temp_dir
                  << ": " << e.what()
                  << ", ignoring manual cleanup required");

    }
    catch(...)
    {
        LOG_ERROR("Exception trying to remove the tmp dir: " << temp_dir
                  << ", ignoring manual cleanup required");
    }

}

namespace
{

std::unique_ptr<Connection::ValueType>
OpenASCO(const Connection::KeyType& key)
{
    return std::make_unique<youtils::FileDescriptor>(key,
                                                     youtils::FDMode::Read,
                                                     CreateIfNecessary::F);
}

}

bool
Connection::partial_read_(const Namespace& ns,
                          const PartialReads& partial_reads,
                          InsistOnLatestVersion)
{
    if (enable_partial_read_ == EnablePartialRead::T)
    {
        for(const auto& partial_read : partial_reads)
        {
            auto sio = lruCache().find(objectPath_(ns,
                                                   partial_read.first),
                                       OpenASCO);

            for (const auto& slice : partial_read.second)
            {
                size_t res = sio->pread(slice.buf,
                                        slice.size,
                                        slice.offset);
                if(res != slice.size)
                {
                    throw BackendRestoreException();
                }
            }
        }

        return true;
    }
    else
    {
        return false;
    }
}

uint64_t
Connection::getSize_(const Namespace& nspace,
                     const std::string& name)
{
    const fs::path p(checkedObjectPath_(nspace, name));
    return fs::file_size(p);
}

void
Connection::listObjects_(const Namespace& nspace,
                         std::list<std::string>& out)
{
    const fs::path p(checkedNamespacePath_(nspace));
    fs::directory_iterator end;

    for (fs::directory_iterator it(p); it != end; ++it)
    {
        if (!fs::is_regular_file(it->status()))
        {
            LOG_WARN("ignoring non-file entry " << it->path());
            continue;
        }

        out.push_back(it->path().filename().string());
    }
}

void
Connection::listNamespaces_(std::list<std::string>& nspaces)
{
    LOCK_BACKEND();

    if(not fs::exists(path_))
    {
        LOG_ERROR("Backend root " << path_ << " does not exist");
        throw BackendBackendException();
    }

    fs::directory_iterator end;
    for(fs::directory_iterator it(path_); it != end; ++it)
    {
        if(fs::is_directory(it->status()))
        {
            nspaces.push_back(it->path().filename().string());
        }
    }
}

fs::path
Connection::nspacePath_(const Namespace& nspace) const
{
    if(not fs::exists(path_))
    {
        LOG_ERROR("Backend root " << path_ << " does not exist");
        throw BackendBackendException();
    }

    return fs::path(path_ / nspace.str());
}

fs::path
Connection::checkedNamespacePath_(const Namespace& nspace) const
{
    const fs::path p(nspacePath_(nspace));
    if (not fs::exists(p))
    {
        throw BackendNamespaceDoesNotExistException();
    }

    return p;
}

fs::path
Connection::objectPath_(const Namespace& nspace,
                        const std::string& objname) const
{
    if(not fs::exists(path_))
    {
        LOG_ERROR("Backend root " << path_ << " does not exist");
        throw BackendBackendException();
    }

    return fs::path(path_ / nspace.str() / objname);
}

fs::path
Connection::checkedObjectPath_(const Namespace& nspace,
                               const std::string& objname) const
{
    const fs::path p(objectPath_(nspace,
                                 objname));
    if (not fs::exists(p))
    {
        if (not fs::exists(nspacePath_(nspace)))
        {
            throw BackendNamespaceDoesNotExistException();
        }
        else
        {
            throw BackendObjectDoesNotExistException();
        }
    }

    return p;
}

backend::ObjectInfo
Connection::x_getMetadata_(const Namespace& nspace,
                           const std::string& name)
{
    LOCK_BACKEND();

    const fs::path p(checkedObjectPath_(nspace, name));

    try
    {
        // XXX:
        // Pretty expensive as the object's read from the filesystem into memory
        // - try to cache metadata or store it with / next to the object.
        std::string tmp;
        std::string md5;
        getFileMD5(p, tmp, md5);
        const size_t size = tmp.size();
        yt::CheckSum chk;
        chk.update(tmp.c_str(), size);
        return ObjectInfo("OK", chk, size, md5);
    }
    catch (std::exception& e)
    {
        LOG_ERROR(p << ": failed to obtain objectmetadata: " << e.what());
        throw BackendRestoreException(); // Fatal/InternalException instead?
    }
    catch (...)
    {
        LOG_ERROR(p << ": failed to obtain objectmetadata: unknown exception");
        throw BackendRestoreException(); // Fatal/InternalException instead?
    }
}

backend::ObjectInfo
Connection::x_setMetadata_(const Namespace& /*nspace*/,
                           const std::string& /*name*/,
                           const backend::ObjectInfo::CustomMetaData& /*metadata*/)
{
    // backend::ObjectInfo object_info;
    // return object_info;
    throw BackendNotImplementedException();
}

backend::ObjectInfo
Connection::x_updateMetadata_(const Namespace& /*nspace*/,
                              const std::string& /*name*/,
                              const backend::ObjectInfo::CustomMetaData& /*metadata*/)
{
    // backend::ObjectInfo object_info;
    // return object_inf;om
    throw BackendNotImplementedException();
}

backend::ObjectInfo
Connection::x_read_(const Namespace& /*nspace*/,
                    const fs::path& /*destination*/,
                    const std::string& /*name*/,
                    InsistOnLatestVersion)
{
    // backend::ObjectInfo object_info;
    // return object_info;
    throw BackendNotImplementedException();
}

void
Connection::getStringMD5(const std::string& input,
                         std::string& md5)
{
    // ASSERTLOCKED
    uint8_t weed[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const byte*>(input.c_str()),
                          input.size(),
                          weed);

    std::stringstream tmp;
    for(int i = 0; i < MD5_DIGEST_LENGTH; ++i)
    {
        unsigned int bt = weed[i];
        tmp << std::hex << std::setfill('0') << std::setw(2) << bt;
    }
    md5 = tmp.str();
}

void
Connection::getFileMD5(const fs::path& input,
                       std::string& destination,
                       std::string& md5)
{
    fs::ifstream ifs(input);
    std::istreambuf_iterator<char> it(ifs);
    while(it != std::istreambuf_iterator<char>())
    {
        destination += *it++;
    }

    getStringMD5(destination,
                 md5);
}

backend::ObjectInfo
Connection::x_read_(const Namespace& nspace,
                    std::string& destination,
                    const std::string& name,
                    InsistOnLatestVersion)
{
    // destination.clear() -- who knows?

    if(name.empty())
    {
        LOG_ERROR("no name specified");
        throw backend::BackendClientException();
    }
    LOCK_BACKEND();

    const fs::path src(checkedObjectPath_(nspace, name));

    try
    {
        std::string etag;
        getFileMD5(src,
                   destination,
                   etag);

        return backend::ObjectInfo("OK",
                                   youtils::FileUtils::calculate_checksum(src),
                                   destination.size(),
                                   etag);
    }
    catch (std::exception& e)
    {
        LOG_INFO(e.what());
        throw backend::BackendRestoreException();
    }
}

backend::ObjectInfo
Connection::x_read_(const Namespace& nspace,
                    std::stringstream& destination,
                    const std::string& name,
                    InsistOnLatestVersion insist_on_latest)
{
    std::string str;
    backend::ObjectInfo o = x_read(nspace,
                                   str,
                                   name,
                                   insist_on_latest);
    destination.str(str);
    return o;
}

backend::ObjectInfo
Connection::x_write_(const Namespace& /*nspace*/,
                     const fs::path& /*location*/,
                     const std::string& /*name*/,
                     const OverwriteObject /*overwrite*/,
                     const backend::ETag* /*etag*/,
                     const youtils::CheckSum* /*chksum*/)
{
    throw BackendNotImplementedException();
}

backend::ObjectInfo
Connection::x_write_(const Namespace& nspace,
                     const std::string& istr,
                     const std::string& name,
                     const OverwriteObject overwrite,
                     const backend::ETag* etag,
                     const youtils::CheckSum* /*chksum*/)
{
    LOCK_BACKEND();

    const fs::path dir(checkedNamespacePath_(nspace));

    const fs::path dst(dir / name);
    if (F(overwrite) and fs::exists(dst))
    {
        LOG_ERROR("Target already in backend " << dst);
        throw backend::BackendOverwriteNotAllowedException();
    }
    if(etag)
    {
        if(fs::exists(dst))
        {
            std::string destination;
            std::string etag_dest;
            getFileMD5(dst,
                       destination,
                       etag_dest);

            if(etag_dest != *etag)
            {
                LOG_ERROR("mismatching etag");
                throw backend::BackendStoreException();
            }
        }
        else
        {
            LOG_ERROR("Etag given but target file does not exist");
            throw backend::BackendStoreException();
        }
    }
    try
    {
        youtils::FileUtils::safe_copy(istr,
                                      dst,
                                      youtils::SyncFileBeforeRename(sync_object_after_write_));

        std::string etag_local;
        getStringMD5(istr,
                     etag_local);

        backend::ObjectInfo oi("OK",
                               youtils::FileUtils::calculate_checksum(dst),
                               istr.size(),
                               etag_local);

        LOG_INFO("Put: " << dst <<
                 ", size " << oi.size_ <<
                 ", chksum " << oi.checksum_ <<
                 ", etag " << etag_local);

        return oi;
    }
    catch (std::exception& e)
    {
        LOG_ERROR("Failed to put " << dst << ": " << e.what());
        throw backend::BackendStoreException();
    }
}

backend::ObjectInfo
Connection::x_write_(const Namespace& nspace,
                     std::stringstream& strm,
                     const std::string& name,
                     OverwriteObject overwrite,
                     const backend::ETag* etag,
                     const youtils::CheckSum* chksum)
{
    return x_write(nspace,
                   strm.str(),
                   name,
                   overwrite,
                   etag,
                   chksum);
}

std::unique_ptr<yt::UniqueObjectTag>
Connection::get_tag_(const Namespace& nspace,
                     const std::string& name)
{
    LOCK_BACKEND();

    const fs::path src(checkedObjectPath_(nspace,
                                          name));

    fs::ifstream ifs(src);
    return std::make_unique<yt::ObjectMd5>(yt::Weed(ifs));
}

std::unique_ptr<yt::UniqueObjectTag>
Connection::read_tag_(const Namespace& nspace,
                      const fs::path& dst,
                      const std::string& name)
{
    LOCK_BACKEND();

    read_(nspace,
          dst,
          name,
          InsistOnLatestVersion::T);

    fs::ifstream ifs(dst);
    return std::make_unique<yt::ObjectMd5>(yt::Weed(ifs));
}

std::unique_ptr<yt::UniqueObjectTag>
Connection::write_tag_(const Namespace& nspace,
                       const fs::path& src,
                       const std::string& name,
                       const yt::UniqueObjectTag* prev_tag,
                       const OverwriteObject overwrite)
{
    if (prev_tag)
    {
        VERIFY(overwrite == OverwriteObject::T);
    }

    LOCK_BACKEND();

    copy_(nspace,
          src,
          name,
          overwrite,
          &name,
          prev_tag);

    fs::ifstream ifs(checkedObjectPath_(nspace,
                                        name));
    return std::make_unique<yt::ObjectMd5>(yt::Weed(ifs));
}

}

}

// Local Variables: **
// mode: c++ **
// End: **
