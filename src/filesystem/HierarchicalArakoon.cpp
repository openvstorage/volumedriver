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

#include "HierarchicalArakoon.h"

namespace volumedriverfs
{

namespace ara = arakoon;
namespace fs = boost::filesystem;
namespace yt = youtils;

const ArakoonEntryId HierarchicalArakoon::root_("root");

HierarchicalArakoon::HierarchicalArakoon(std::shared_ptr<yt::LockedArakoon> arakoon,
                                         const std::string& prefix)
    : arakoon_(arakoon)
    , prefix_(prefix)
{
    LOG_INFO("Arakoon Cluster ID " << arakoon_->cluster_id() << ", prefix: " << prefix_);
}

std::string
HierarchicalArakoon::make_key_(const std::string& k) const
{
    return prefix_ + std::string("/") + k;
}

HierarchicalArakoon::Entry
HierarchicalArakoon::deserialize_entry_(std::istream& is)
{
    IArchive ia(is);
    Entry e;
    ia >> e;
    return e;
}

HierarchicalArakoon::Entry
HierarchicalArakoon::deserialize_entry_(const ara::buffer& buf)
{
    return buf.as_istream<Entry>([&](std::istream& is) -> Entry
        {
            return deserialize_entry_(is);
        });
}

void
HierarchicalArakoon::validate_path_(const fs::path& path)
{
    if (path.empty())
    {
        LOG_ERROR("The empty path is not supported");
        throw InvalidPathException("The empty path is not supported",
                                   "(empty)",
                                   EINVAL);
    }

    if (path.is_relative())
    {
        LOG_ERROR("relative paths are not supported");
        throw InvalidPathException("empty path not supported",
                                   path.string().c_str(),
                                   EINVAL);
    }

    for (const auto& s : path)
    {
        if (s.string() == "." or
            s.string() == "..")
        {
            LOG_ERROR("paths containing \".\" or \"..\" components or trailing slashes are not supported");
            throw InvalidPathException("paths containing \".\" or \"..\" components or trailing slashes are not supported",
                                       path.string().c_str(),
                                       EINVAL);
        }
    }
}

ara::buffer
HierarchicalArakoon::do_find_(const fs::path& path)
{
    LOG_TRACE(path);

    try
    {
        ara::buffer buf;

        const std::string rkey(make_key_(root_));
        buf = arakoon_->get(rkey);

        for (const auto& s : path)
        {
            const Entry entry(deserialize_entry_(buf));

            try
            {
                const std::string key(make_key_(entry.at(s.string()).str()));
                buf = arakoon_->get(key);
            }
            catch (std::out_of_range&)
            {
                LOG_DEBUG(path << ": component " << s << " does not exist");
                throw DoesNotExistException("Path does not exist",
                                            path.string().c_str(),
                                            ENOENT);
            }
        }

        LOG_TRACE("found " << path);

        return buf;
    }
    catch (ara::error_not_found&)
    {
        LOG_ERROR(path << ": does not exist");
        throw DoesNotExistException("Path does not exist",
                                    path.string().c_str(),
                                    ENOENT);
    }
}

ara::buffer
HierarchicalArakoon::find_parent_(const fs::path& path)
{
    LOG_TRACE(path);

    validate_path_(path);
    return do_find_(path.parent_path());
}

ara::buffer
HierarchicalArakoon::find_(const fs::path& path)
{
    LOG_TRACE(path);

    validate_path_(path);
    return do_find_(path);
}

bool
HierarchicalArakoon::initialized()
{
    LOG_TRACE("checking whether cluster ID " << arakoon_->cluster_id() << ", prefix " <<
              prefix_ << " is initialized");

    try
    {
        find_("/");
        LOG_TRACE("is initialized");
        return true;
    }
    catch(DoesNotExistException&)
    {
        LOG_TRACE("is not initialized");
        return false;
    }
}

std::list<std::string>
HierarchicalArakoon::list(const ArakoonPath& path)
{
    LOG_TRACE(path);

    const Entry entry(deserialize_entry_(find_(path)));

    std::list<std::string> l;
    for (const auto& v : entry)
    {
        l.push_back(v.first);
    }

    return l;
}

std::list<std::string>
HierarchicalArakoon::list(const yt::UUID& id)
{
    LOG_TRACE(id);

    ara::buffer buf;

    const std::string rkey(make_key_(id.str()));
    buf = arakoon_->get(rkey);

    const Entry entry(deserialize_entry_(buf));

    std::list<std::string> l;
    for (const auto& v : entry)
    {
        l.push_back(v.first);
    }

    return l;
}

std::string
HierarchicalArakoon::update_serialized_entry_(const HierarchicalArakoon::Entry& entry,
                                              std::istream& is)
{
    std::stringstream ss;

    OArchive oa(ss);
    oa << entry;

    ss << is.rdbuf();
    return ss.str();
}

void
HierarchicalArakoon::do_prepare_erase_sequence_(const ara::buffer& pbuf,
                                                Entry& pentry,
                                                std::istream& is,
                                                const std::string& name,
                                                boost::optional<std::string>& pkey,
                                                ara::sequence& seq)
{
    auto it = pentry.find(name);
    if (it == pentry.end())
    {
        LOG_ERROR(name << ": does not exist");
        throw DoesNotExistException("Entry does not exist ",
                                     name.c_str(),
                                     ENOENT);
    }

    const std::string key(make_key_(it->second));
    const ara::buffer buf(arakoon_->get(key));
    const Entry entry(deserialize_entry_(buf));

    if (not entry.empty())
    {
        LOG_ERROR(name << ": still " << entry.size() << " children");
        throw NotEmptyException("Entry still has children ",
                                 name.c_str(),
                                 ENOTEMPTY);
    }

    pentry.erase(it);
    seq.add_assert(*pkey, pbuf);
    seq.add_assert(key, buf);
    seq.add_set(*pkey, update_serialized_entry_(pentry, is));
    seq.add_delete(key);
}


void
HierarchicalArakoon::prepare_erase_sequence_(const fs::path& path,
                                             boost::optional<std::string>& pkey,
                                             ara::sequence& seq)
{
    LOG_TRACE(path << ", pkey " << pkey);

    with_parent_<void>(path,
                       pkey,
                       [&](const ara::buffer& pbuf,
                           Entry& pentry,
                           std::istream& is)
                       {
                            do_prepare_erase_sequence_(pbuf,
                                                       pentry,
                                                       is,
                                                       path.filename().string(),
                                                       pkey,
                                                       seq);
                       });
}

void
HierarchicalArakoon::prepare_erase_sequence_(const yt::UUID& parent_id,
                                             const std::string& name,
                                             ara::sequence& seq)
{
    LOG_TRACE(parent_id << ", name" << name);

    with_parent_<void>(parent_id,
                       [&](const ara::buffer& pbuf,
                           Entry& pentry,
                           std::istream& is)
                       {
                            boost::optional<std::string> pkey(make_key_(parent_id.str()));

                            do_prepare_erase_sequence_(pbuf,
                                                       pentry,
                                                       is,
                                                       name,
                                                       pkey,
                                                       seq);
                       });
}

void
HierarchicalArakoon::erase(const ArakoonPath& path)
{
    LOG_TRACE("Attempting to remove " << path);

    boost::optional<std::string> pkey;

    arakoon_->run_sequence("erase entry",
                           [&](ara::sequence& seq)
                           {
                               prepare_erase_sequence_(path, pkey, seq);
                           },
                           yt::RetryOnArakoonAssert::T);
}

void
HierarchicalArakoon::erase(const yt::UUID& parent_id,
                           const std::string& name)
{
    LOG_TRACE("Attempting to remove " << name);

    arakoon_->run_sequence("erase entry",
                           [&](ara::sequence& seq)
                           {
                               prepare_erase_sequence_(parent_id, name, seq);
                           },
                           yt::RetryOnArakoonAssert::T);
}

bool
HierarchicalArakoon::is_prefix_(const fs::path& pfx, const fs::path& path)
{
    fs::path p(path);

    while (not p.empty())
    {
        if (p == pfx)
        {
            return true;
        }
        else
        {
            p = p.parent_path();
        }
    }

    return false;
}

void
HierarchicalArakoon::destroy(std::shared_ptr<yt::LockedArakoon> larakoon,
                             const std::string& prefix)
{
    LOG_INFO("Destroying HierarchicalArakoon, prefix " << prefix);
    larakoon->delete_prefix(prefix);
}

}
