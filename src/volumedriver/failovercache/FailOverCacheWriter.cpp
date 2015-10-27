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
#include <youtils/Assert.h>
#include <boost/scope_exit.hpp>
#include <boost/scoped_array.hpp>
#include "FailOverCacheProtocol.h"
#include "failovercache/fungilib/WrapByteArray.h"

namespace failovercache
{
using namespace volumedriver;
using namespace fungi;


FailOverCacheWriter::FailOverCacheWriter(const fs::path& root,
                                         const std::string& ns,
                                         const volumedriver::ClusterSize& cluster_size)
    : registered_(false)
    , first_command_must_be_getEntries(false)
    , root_(root)
    , ns_(ns)
    , relFilePath_(root / ns )
    , f_(0)
    , cluster_size_(cluster_size)
    , check_offset(0)
{
    fs::create_directories(relFilePath_);
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
    for(sco_iterator it = scosdeque_.begin();
        it != scosdeque_.end();
        ++it)
    {
        fs::path path = makePath(*it);
        LOG_DEBUG("removing failover cache data for " << *it << " at " << path << " for namespace " << ns_);
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

// void
// FailOverCacheWriter::unregister_()
// {
//     LOG_DEBUG("Unregistering for namespace " << ns_);

//     registered_ = false;
// }

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
FailOverCacheWriter::makePath(const volumedriver::SCO sconame) const
{

    return root_ / ns_ / sconame.str();
}

void
FailOverCacheWriter::addEntry(volumedriver::ClusterLocation cl,
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

        fungi::IOBaseStream fstream(f_);
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
FailOverCacheWriter::getEntries(FailOverCacheProtocol* prot) const
{
    LOG_DEBUG("Getting entries for namespace " << ns_);
    boost::scoped_array<byte> buf(new byte[cluster_size_]);
    fflush(0);

    for (sco_const_iterator it = scosdeque_.begin();
         it != scosdeque_.end();
         ++it)
    {

        LOG_DEBUG("Sending SCO " << *it);
        const fs::path filename = makePath(*it);

        VERIFY(fs::exists(filename));

        File f(filename.string(), fungi::File::Read);
        f.open();
        IOBaseStream fstream(&f);
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

            prot->processFailOverCacheEntry(cl,lba,buf.get(), len);
        }
        fstream.close();
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
                            FailOverCacheProtocol* prot) const
{
    LOG_DEBUG("Getting SCO " << std::hex << sconame << std::dec <<
              " for namespace " << ns_);

    sco_const_iterator it = std::find(scosdeque_.begin(), scosdeque_.end(),sconame);
    if(it == scosdeque_.end())
    {
        return;
    }
    else
    {
        boost::scoped_array<byte> buf(new byte[cluster_size_]);
        if (f_)
        {
            f_->flush();
        }
        LOG_INFO("Sending SCO " << *it);
        const fs::path filename = makePath(*it);

        VERIFY(fs::exists(filename));

        File f(filename.string(), fungi::File::Read);
        f.open();
        IOBaseStream fstream(&f);
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

            LOG_DEBUG("Sending entry " << cl << ", lba " << lba << " for namespace " << ns_);
            prot->processFailOverCacheSCO(cl,lba,buf.get(), len);
        }
        fstream.close();
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
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
