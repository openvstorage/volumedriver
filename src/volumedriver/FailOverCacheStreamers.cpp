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

#include "FailOverCacheStreamers.h"
#include "failovercache/fungilib/WrapByteArray.h"
#include "failovercache/fungilib/Socket.h"

namespace
{
DECLARE_LOGGER("FailOverCacheProtocol");
}

namespace volumedriver
{

fungi::IOBaseStream&
checkStreamOK(fungi::IOBaseStream& stream,
              const std::string& ex)
{
    stream >> fungi::IOBaseStream::cork;
    uint32_t res;
    stream >> res;
    if(res != volumedriver::Ok)
    {
        LOG_ERROR("FailOverCache Protocol Error: *Not* Ok returned in " << ex);
        throw fungi::IOException(ex.c_str());
    }
    return stream;
}

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream& stream, const CommandData<Register>& data)
{

    stream << fungi::IOBaseStream::cork;
    OUT_ENUM(stream, Register);
    stream << data.ns_;
    stream << static_cast<uint32_t>(data.clustersize_);
    stream << fungi::IOBaseStream::uncork;
    return checkStreamOK(stream, "Register");
}


 fungi::IOBaseStream&
operator>>(fungi::IOBaseStream& stream, CommandData<Register>& data)
{
    stream >> data.ns_;
    stream >> data.clustersize_;
    return stream;
}


fungi::IOBaseStream&
operator<<(fungi::IOBaseStream& stream, const CommandData<AddEntries>& data)
{
    bool isRDMA = stream.isRdma();
    stream << fungi::IOBaseStream::cork;
    OUT_ENUM(stream, AddEntries);
    const size_t wsize = data.entries_.size();
    stream << wsize;
    if (isRDMA) // make small but finished packets with RDMA
    {
        stream << fungi::IOBaseStream::uncork;
    }

    for (boost::ptr_vector<FailOverCacheEntry>::const_iterator it = data.entries_.begin();
        it!= data.entries_.end();
        ++it)
    {
        if (isRDMA) // make small but finished packets with RDMA
        {
            stream << fungi::IOBaseStream::cork;
        }
        stream << it->cli_;
        stream << it->lba_;
        stream << fungi::WrapByteArray(it->buffer_.get(), it->size_);
        if (isRDMA) // make small but finished packets with RDMA
        {
            stream << fungi::IOBaseStream::uncork;
        }
    }
    stream << fungi::IOBaseStream::uncork;
    return checkStreamOK(stream, "AddEntries");
}

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream& stream, CommandData<AddEntries>& data)
{
    size_t size;
    stream >> size;
    data.entries_.reserve(size);

    for(size_t i = 0; i < size; ++i)
    {
        data.entries_.push_back(new FailOverCacheEntry());

//        stream >> fungi::IOBaseStream::cork;
        stream >> data.entries_.back().cli_;
        stream >> data.entries_.back().lba_;
        int32_t size32 = 0;
        data.entries_.back().buffer_.reset(stream.readByteArray(size32));
        assert(size32 > 0);
        data.entries_.back().size_ = size32;
    }

    return stream;
}

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream& stream, const CommandData<Flush>& /*data*/)
{
    stream << fungi::IOBaseStream::cork;
    OUT_ENUM(stream, Flush);
    stream << fungi::IOBaseStream::uncork;
    return checkStreamOK(stream,"Flush");
}


fungi::IOBaseStream&
operator>>(fungi::IOBaseStream& stream, CommandData<Flush>& /*data*/)
{
    return stream;
}

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream& stream, const CommandData<Clear>& /*data*/)
{
    stream << fungi::IOBaseStream::cork;
    OUT_ENUM(stream, Clear);
    stream << fungi::IOBaseStream::uncork;
    return checkStreamOK(stream, "Clear");
}
}

// Local Variables: **
// mode: c++ **
// End: **
