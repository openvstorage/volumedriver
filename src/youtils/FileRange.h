// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FILE_RANGE_H_
#define FILE_RANGE_H_
#include "FileDescriptor.h"
#include "Assert.h"
#include <iosfwd>
#include <memory>

#include <boost/scope_exit.hpp>
#include <boost/iostreams/positioning.hpp>
#include <boost/iostreams/categories.hpp>
#include <boost/lexical_cast.hpp>
namespace youtils
{

namespace bio = boost::iostreams;

MAKE_EXCEPTION(FileRangeViolation, fungi::IOException);

template<typename F>
class FileRange
{
public:
    typedef bio::seekable_device_tag category;
    typedef char char_type;
    typedef std::shared_ptr<F> file_ptr;

    FileRange(file_ptr f,
              uint64_t off,
              uint64_t sz)
        : offset(off)
        , size(sz)
        , f_(f)
        , pos_(0)
    {
        if (offset + size > f_->size())
        {
            throw FileRangeViolation("requested file range outside the file's size",
                                     f_->path().string().c_str());
        }
    }

    ~FileRange() = default;

    FileRange(const FileRange&) = default;

    FileRange&
    operator=(const FileRange&) = default;

    ssize_t
    pwrite(const void* buf,
           size_t sz,
           off_t off)
    {
        check_range_(off, sz);
        return f_->pwrite(buf,
                          sz,
                          offset + off);
    }

    ssize_t
    pread(void* buf,
          size_t sz,
          off_t off)
    {
        check_range_(off, sz);
        return f_->pread(buf,
                         sz,
                         offset + off);
    }

    // XXX: verify EOF / error semantics
    std::streamsize
    read(char_type* buf, std::streamsize n)
    {
        ASSERT(pos_ <= size);

        // really check for n > 0?
        if (n > 0 and pos_ == size)
        {
            return -1; // EOF
        }

        // we do allow short reads here (very likely caused by the stream buffer).
        // XXX: reconsider short reads for pread?
        const size_t sz = std::min<uint64_t>(n, size - pos_);
        ssize_t r = pread(buf, sz, pos_);
        VERIFY(r >= 0);

        ASSERT(pos_ <= size);
        pos_ += r;
        return r;
    }

    // XXX: verify EOF / error semantics. In particular for write: can the stream
    // buffer more data than the FileRange can hold, and do we care at this point?
    std::streamsize
    write(const char_type* buf, std::streamsize n)
    {
        ASSERT(pos_ <= size);

        // really check for n > 0?
        if (n > 0 and pos_ == size)
        {
            return -1; // EOF
        }
        const size_t sz = std::min<uint64_t>(n, offset + size - pos_);
        ssize_t r = pwrite(buf, sz, pos_);
        VERIFY(r >= 0);
        pos_ += r;

        ASSERT(pos_ <= size);
        return r;
    }

    bio::stream_offset
    seek(bio::stream_offset off, std::ios_base::seekdir way)
    {
        ASSERT(pos_ <= size);

        bio::stream_offset next;
        switch (way)
        {
        case std::ios_base::beg:
            next = off;
            break;
        case std::ios_base::cur:
            next = pos_ + off;
            break;
        case std::ios_base::end:
            next = size + off;
            break;
        default:
            VERIFY(0 == "impossible code path");
            break;
        }

        check_range_(next, 0);
        pos_ = next;
        ASSERT(pos_ <= size);
        return pos_;
    }

    const std::string
    name() const
    {
        return f_->path().string() + std::string("@") +
            boost::lexical_cast<std::string>(offset) + std::string("+") +
            boost::lexical_cast<std::string>(size);
    }

    uint64_t
    capacity() const
    {
        return size;
    }

private:
    DECLARE_LOGGER("FileRange");

    const uint64_t offset;
    const uint64_t size;

    file_ptr f_;
    uint64_t pos_; // zero-based

    void
    check_range_(const off_t off, const size_t sz) const
    {
        if (offset + off + sz > offset + size)
        {
            throw FileRangeViolation("requested file range outside the file's size",
                                     f_->path().string().c_str());
        }
    }
};

}

#endif // !FILE_RANGE_H_

// Local Variables: **
// mode: c++ **
// End: **
