// Copyright 2015 iNuron NV
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

#include "FailOverCacheWriter.h"
#include "failovercache/fungilib/WrapByteArray.h"

#include <boost/scoped_array.hpp>

#include <youtils/Assert.h>

namespace failovercache
{

using namespace volumedriver;
using namespace fungi;

namespace fs = boost::filesystem;

FailOverCacheWriter::FailOverCacheWriter(const fs::path& root,
                                         const std::string& ns,
                                         const ClusterSize& cluster_size)
    : registered_(false)
    , first_command_must_be_getEntries(false)
    , root_(root)
    , ns_(ns)
    , f_(0)
    , cluster_size_(cluster_size)
    , check_offset(0)
{
    fs::create_directories(root / ns);
}

FailOverCacheWriter::~FailOverCacheWriter()
{
    try
    {
        scosdeque_.clear();
        fs::remove_all(root_ / ns_);
    }
    catch(std::exception& e)
    {
        LOG_WARN("Removing directory " << (root_ / ns_ ) << " failed, " << e.what());
    }
    catch(...)
    {
        LOG_WARN("Removing directory " << (root_ / ns_ ) << " failed, unknown exception");

    }

    delete f_;
}

void
FailOverCacheWriter::ClearCache()
{
    LOG_DEBUG("Namespace: " << ns_);
    for (auto sco : scosdeque_)
    {
        fs::path path = makePath(sco);
        LOG_DEBUG("removing failover cache data for " << sco << " at " << path << " for namespace " << ns_);
        fs::remove(path);
    }
    if(f_)
    {
        f_->close();
        delete f_;
        f_ = 0;
    }
    scosdeque_.clear();
}

void
FailOverCacheWriter::removeUpTo(const SCO sconame)
{

    LOG_INFO("Up to SCO " << sconame << " Namespace: " << ns_);
    VERIFY(sconame.version() == 0);
    VERIFY(sconame.cloneID() == 0);

    if(!scosdeque_.empty())
    {
        SCONumber sconum = sconame.number();
        SCONumber oldest = scosdeque_.front().number();

        LOG_DEBUG("Oldest SCONUmber in cache " << oldest << " sconumber passed: " << sconum << ", namespace: " << ns_);
        if( sconum < oldest)
        {
            LOG_DEBUG("not removing anything for namespace: " << ns_);
            return;
        }

        if(f_ and sconame == scosdeque_.back())
        {
            LOG_DEBUG("Closing filedescriptor" << ns_);
            f_->close();
            delete f_;
            f_ = 0;
        }
        while(not scosdeque_.empty() and
              sconum >= scosdeque_.front().number())
        {
            fs::path path = makePath(scosdeque_.front());
            LOG_INFO("Removing SCO " << path);
            fs::remove(path);
            scosdeque_.pop_front();
        }
    }
}

fs::path
FailOverCacheWriter::makePath(const SCO sconame) const
{
    return root_ / ns_ / sconame.str();
}

void
FailOverCacheWriter::addEntries(std::vector<FailOverCacheEntry> entries,
                                std::unique_ptr<uint8_t[]> buf)
{
    for (auto& e : entries)
    {
        addEntry(e.cli_,
                 e.lba_,
                 // AR: the WrapByteArray stuff in the call chain needs this. Fix it!
                 const_cast<uint8_t*>(e.buffer_),
                 e.size_);
    }
}

void
FailOverCacheWriter::addEntry(ClusterLocation cl,
                              const uint64_t lba,
                              byte* buffer,
                              uint32_t size)
{
    SCO sconame = cl.sco();
    VERIFY(cl.version() == 0);
    VERIFY(cl.cloneID() == 0);

    SCOOffset scooffset = cl.offset();

    LOG_DEBUG("Adding entry clusterlocation " << cl << ", lba "  << lba  << " for namespace " << ns_);
    if(first_command_must_be_getEntries)
    {
        ClearCache();
        first_command_must_be_getEntries = false;
    }
    try
    {
        if(scosdeque_.empty() or
           scosdeque_.back() != sconame)
        {
            if (f_)
            {
                f_->close();
            }

            delete f_;
            fs::path scopath = makePath(sconame);

            LOG_INFO("opening new sco: " << scopath);

            f_ = new fungi::File(scopath.string(),
                                 fungi::File::Append);
            f_->open();
            scosdeque_.push_back(sconame);
            check_offset = scooffset;
        }
        else
        {
            VERIFY(scooffset == ++check_offset);
        }

        fungi::IOBaseStream fstream(*f_);
        fstream << cl << lba << fungi::WrapByteArray(buffer, size);

    }
    catch(...)
    {
        LOG_ERROR("add Entry error");
        throw;
    }
}

void
FailOverCacheWriter::Clear()
{
    ClearCache();
    first_command_must_be_getEntries = false;
}

void
FailOverCacheWriter::process_sco_(const SCO sco,
                                  EntryProcessorFun& fun)
{

    LOG_INFO(ns_ << ": processing SCO " << sco);

    const fs::path filename = makePath(sco);

    VERIFY(fs::exists(filename));

    File f(filename.string(), fungi::File::Read);
    f.open();
    IOBaseStream fstream(f);

    boost::scoped_array<byte> buf(new byte[cluster_size_]);

    while (!f.eof())
    {
        ClusterLocation cl;
        fstream >> cl;

        uint64_t lba;
        fstream >> lba;

        int64_t bal; // byte array length
        fstream >> bal;
        int32_t len = (int32_t) bal;
        VERIFY(len == static_cast<int32_t>(cluster_size_));
        fstream.read(buf.get(), len);

        LOG_DEBUG("Sending entry " << cl
                  << ", lba " << lba
                  << " for namespace " << ns_);

        fun(cl,
            lba,
            buf.get(),
            len);
    }

    fstream.close();
}

void
FailOverCacheWriter::getEntries(EntryProcessorFun fun)
{
    LOG_DEBUG("Getting entries for namespace " << ns_);
    fflush(0);

    for (const auto& sco : scosdeque_)
    {
        process_sco_(sco,
                     fun);
    }
    first_command_must_be_getEntries = false;
}

void
FailOverCacheWriter::getSCORange(SCO& oldest,
                                 SCO& youngest) const
{
    LOG_DEBUG("Getting SCORange for for namespace " << ns_);
    if(not scosdeque_.empty())
    {
        oldest= scosdeque_.front();
        youngest = scosdeque_.back();
    }
    else
    {
        oldest = SCO();
        youngest = SCO();
    }
}

void
FailOverCacheWriter::getSCO(SCO sconame,
                            EntryProcessorFun fun)
{
    LOG_DEBUG("Getting SCO " << std::hex << sconame << std::dec <<
              " for namespace " << ns_);

    const auto it = std::find(scosdeque_.begin(), scosdeque_.end(),sconame);
    if(it == scosdeque_.end())
    {
        return;
    }
    else
    {
        Flush();

        process_sco_(*it,
                     fun);
    }
}

void
FailOverCacheWriter::Flush()
{
    if (f_)
    {
        f_->flush();
    }
}

}
// Local Variables: **
// mode: c++ **
// End: **
