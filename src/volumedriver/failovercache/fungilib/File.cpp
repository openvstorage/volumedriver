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

#define INSIDE_FILE_CPP
#include "File.h"
#include "ByteArray.h"

#include <cstring>
#include <string>
#include <cerrno>
#include <iostream>
#include <limits>
#include <cstdlib>
#include <cerrno>

#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <cassert>

#include <youtils/Assert.h>
#include <youtils/IOException.h>

namespace fungi {

File::File(const std::string &filename, int mode) :
    filename_(filename),
    f_(nullptr),
    mode_(mode)
{
}

void
File::open(boost::optional<size_t> bufsize)
{
    VERIFY(f_ == nullptr);
    // the reason for the "F" below is that in Solaris 32-bit this is needed
    // to allow files to have an underlying filedescriptor >= 256
    // this is for legacy reasons and is explained more in detail in the
    // manual page of fopen on solaris.
    int mode = mode_;
    const char *modes = 0;

    switch (mode)
    {
    case Append:
        modes = "abF";
        break;
    case (Append|Read):
        modes = "a+bF";
        break;
    case Read:
        modes = "rbF";
        break;
    case Write:
        modes = "wbF";
        break;
    case (Read|Write):
        modes = "w+bF";
        break;
    case Update:
        modes = "r+bF";
        break;
    default:
        throw IOException("File::open: unsupported mode", filename_.c_str(), 0);
    }

    if (bufsize and *bufsize)
    {
        buf_ = std::make_unique<char[]>(*bufsize);
    }
    else
    {
        buf_ = nullptr;
    }

    f_ = fopen(filename_.c_str(), modes);
    if (f_ == nullptr) {
        throw IOException("File::open", filename_.c_str(), errno);
    }

    if (bufsize)
    {
        setbuffer(f_, buf_.get(), *bufsize);
    }
}

File::~File()
{
    closeNoThrow();
}

void File::close() {
    if (f_ and fclose(f_) == EOF) {
        f_ = nullptr;
        throw IOException("File::close", filename_.c_str(), errno);
    }

    f_ = nullptr;
}

void File::closeNoThrow() {
    if (f_)
    {
        fclose(f_);
        f_ = nullptr;
    }
}

size_t
File::saneRead(void* ptr,
               size_t size,
               size_t count)
{
    assert(f_);

    size_t res = fread(ptr, size, count, f_);
    if(ferror(f_))
    {
        clearerr(f_);
        throw IOException("File::read", filename_.c_str(), errno);
    }
    return res;
}

int32_t
File::read(byte *ptr,
           const int32_t count)
{
    // Will take somebody with a larger supply of hallicunogenics to understand this code.
    int32_t res = fread(ptr, sizeof(byte), (size_t)count, f_);


    if (feof(f_) && res == 0 && count == 0)
    {
        throw IOException("File::read EOF", filename_.c_str(), 0);
    }
    int err = ferror(f_);

    if (err != 0)
    {
        clearerr(f_);
        throw IOException("File::read", filename_.c_str(), err);
    }

    return res;
}

byte File::peek() {
    const int res = fgetc(f_);
    if (res == EOF) {
        throw IOException("File::peek EOF", filename_.c_str(), 0);
    }
    const byte bres = (byte)res;
    const int res2 = ungetc(res, f_);
    if (res2 == EOF) {
        throw IOException("File::peek EOF on ungetc", filename_.c_str(), 0);
    }
    return bres;
}

bool File::eof() const {
    const int res = fgetc(f_);
    if (res == EOF) {
        return true;
    }
    const int res2 = ungetc(res, f_);
    if (res2 == EOF) {
        throw IOException("File::eof EOF on ungetc", filename_.c_str(), 0);
    }
    return false;
}

int32_t
File::write(const byte *ptr,
            int32_t count)
{
    ASSERT(f_);
    // LOG_WARN("XXX writing " << count << "bytes");
    // here count is casted to size_t, giving a possible loss
    // however we can safely assume that no-one wants to read more then 2**31-1 bytes at once
    int32_t res = fwrite(ptr, sizeof(byte), (size_t)count, f_);
    int err = ferror(f_);
    // Dont pass the err into the error here since it is apparently meaningless.
    if (err != 0) {
        clearerr(f_);
        throw IOException("File::write", filename_.c_str());
    }

    return res;

}

int32_t File::write(const ByteArray &data) {
	ASSERT(f_);
	return write(data, data.size());
}

int File::fileno() const {
	if (f_ == 0) {
		throw IOException("File::fileno: file handle is 0");
	}
	int no = ::fileno(f_);
	if (no == -1) {
		throw IOException("File::fileno", filename_.c_str(), errno);
	}
	return no;
}

void File::flush() {
	if (f_ == 0) {
		throw IOException("File::flush, no file handle available");
	}
    int err = fflush(f_);
    if (err != 0) {
        throw IOException("File::flush", filename_.c_str(), errno);
    }
}

void File::sync() {
    if (fsync(fileno()) < 0) {
    	throw IOException("File::sync", filename_.c_str(), errno);
    }
}

void File::datasync() {
    if (fdatasync(fileno()) < 0) {
    	throw IOException("File::datasync", filename_.c_str(), errno);
    }
}

FILE *File::getHandle() const {
	return f_;
}

int64_t File::length() const
{
	int64_t length, cur_pos;
	cur_pos = tell();
	if (fseeko(f_, 0, SEEK_END) == -1) {
		throw IOException("File::length", filename_.c_str(), errno);
	}
	length = tell();
    if(fseeko(f_,cur_pos,SEEK_SET))
    {
        throw IOException("seek", filename_.c_str(), errno);
    }
	return length;
}

int64_t File::length(const std::string &filename) {
    File file(filename);
    file.open();
    int64_t l = file.length();
    file.close();
    return l;
}

void File::rewind() {
    ASSERT(f_);
    ::rewind(f_);
}

int64_t File::tell() const
{
    if (f_ == nullptr)
    {
        return File::length(filename_);
    }
    const int64_t cur_pos = ftello(f_);
    if (cur_pos == -1) {
        throw IOException("length", filename_.c_str(), errno);
    }
    return cur_pos;
}

void File::seek(int64_t offset, int WHENCE) {
    ASSERT(f_);
    if (fseeko(f_, offset, WHENCE) != 0) {
        throw IOException("seek", filename_.c_str(), errno);
    }
}

const std::string &File::name() const {
	return filename_;
}

void File::truncate(int64_t length) {
	flush();
	int res = ftruncate(fileno(), length);
	if (res < 0) {
		throw IOException("ftruncate", filename_.c_str(), errno);
	}
}

}
// Local Variables: **
// End: **
