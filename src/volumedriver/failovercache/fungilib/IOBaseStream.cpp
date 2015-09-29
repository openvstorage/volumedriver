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

#include "Conversions.h"
#include "IOBaseStream.h"
#include <youtils/IOException.h>
#include <cassert>
#include <cstdlib>
#include <sstream>

namespace fungi {
IOBaseStream::Cork IOBaseStream::cork;
IOBaseStream::Uncork IOBaseStream::uncork;
static const int STREAM_MAX_STRING_SIZE = 1024;
static const int STREAM_MAX_BYTEARRAY_SIZE = 64 * 1024;

IOBaseStream &IOBaseStream::operator<<(byte i) {
	writeByte_(i);
	return *this;
}

IOBaseStream &IOBaseStream::operator<<(bool i) {
	writeboolean_(i);
	return *this;
}

IOBaseStream &IOBaseStream::operator<<(int32_t i) {
	writeInt_(i);
	return *this;
}

IOBaseStream &IOBaseStream::operator<<(uint32_t i)
{
    write(reinterpret_cast<byte*>(&i),sizeof(i));
    return *this;
}

IOBaseStream &IOBaseStream::operator<<(uint16_t i)
{
    write(reinterpret_cast<byte*>(&i),sizeof(i));
    return *this;
}

IOBaseStream &IOBaseStream::operator<<(uint64_t i)
{
    write(reinterpret_cast<byte*>(&i),sizeof(i));
    return *this;
}

IOBaseStream &IOBaseStream::operator<<(int64_t i) {
	writeLong_(i);
	return *this;
}

IOBaseStream& IOBaseStream::operator <<(const float number) {
	writeFloat_(number);
	return *this;

}

IOBaseStream& IOBaseStream::operator <<(const double number) {
	writeDouble_(number);
	return *this;

}

IOBaseStream &IOBaseStream::operator<<(const std::string &i) {
	writeString_(&i);
	return *this;
}

IOBaseStream &IOBaseStream::operator<<(const ByteArray &i) {
	writeByteArray_(&i);
	return *this;
}

IOBaseStream &IOBaseStream::operator>>(int32_t &i) {
	i = readInt_();
	return *this;
}

IOBaseStream &IOBaseStream::operator>>(uint64_t &i)
{
    read(reinterpret_cast<byte*>(&i), sizeof(i));
    return *this;
}

IOBaseStream &IOBaseStream::operator>>(uint32_t &i)
{
    read(reinterpret_cast<byte*>(&i), sizeof(i));
    return *this;
}

IOBaseStream &IOBaseStream::operator>>(uint16_t &i)
{
    read(reinterpret_cast<byte*>(&i), sizeof(i));
    return *this;
}

IOBaseStream &IOBaseStream::operator>>(int64_t &i) {
	i = readLong_();
	return *this;
}

IOBaseStream &IOBaseStream::operator>>(bool &i) {
	i = readboolean_();
	return *this;
}

IOBaseStream &IOBaseStream::operator>>(byte &i) {
	i = readByte_();
	return *this;
}

IOBaseStream& IOBaseStream::operator >>(float& number) {
	number = readFloat_();
	return *this;

}

IOBaseStream& IOBaseStream::operator >>(double& number) {
	number = readDouble_();
	return *this;

}

IOBaseStream &IOBaseStream::operator>>(std::string &i) {
	i = readString_();
	return *this;
}


IOBaseStream &IOBaseStream::operator<<(const IOBaseStream::Null &) {
	writeString_(0);
	return *this;
}

IOBaseStream &IOBaseStream::operator<<(const Cork &) {
	setCork_();
	return *this;
}

IOBaseStream &IOBaseStream::operator<<(const Uncork &) {
	clearCork_();
	return *this;
}

IOBaseStream &IOBaseStream::operator>>(const Cork &) {
    getCork_();
    return *this;
}

IOBaseStream &IOBaseStream::operator<<(const RequestTimeout& r){
    mySink_->setRequestTimeout(r.getRequestTimeout());
    return *this;
}

void IOBaseStream::writeInt_(int32_t i) {
	byte buf[Conversions::intSize];
	Conversions::bytesFromInt(buf, i);
	write(buf, Conversions::intSize);
}

void IOBaseStream::writeLong_(int64_t l) {
	byte buf[Conversions::longSize];
	Conversions::bytesFromLong(buf, l);
	write(buf, Conversions::longSize);
}

void IOBaseStream::writeboolean_(bool b) {
	byte buf[1];
	buf[0] = b;
	write(buf, 1);
}

void IOBaseStream::writeByte_(byte b) {
	write(&b, 1);
}

void IOBaseStream::writeDouble_(double number) {
	std::stringstream ss;
	ss << number;
	std::string text = ss.str();
	writeString_(&text);
}

void IOBaseStream::writeFloat_(float number) {
	std::stringstream ss;
	ss << number;
	std::string text = ss.str();
	writeString_(&text);
}

void IOBaseStream::writeString_(const std::string *s) {
	if (s == 0) {
		writeLong_(-1);
	} else {
		writeLong_(s->size());
		if (s->size() > 0) {
			write((const byte *) s->c_str(), s->size());
			LOG_DEBUG("write to socket string: " << s->c_str() << " with size: " << s->size());
		}
	}
}

void IOBaseStream::writeByteArray_(const ByteArray *array) {
	if (array == 0) {
		writeLong_(-1);
	} else {
		writeLong_(array->size());
		if (array->size() > 0) {
			write(*array, array->size());
		}
	}
}

byte* IOBaseStream :: readByteArray(int32_t &s32){
    int64_t size = readLong_();
    byte* buffer=0;
    if(size == -1)
    {
        s32 = 0;
    }
    else
    {
        if (size > STREAM_MAX_BYTEARRAY_SIZE)
        {
            LOG_ERROR("Got " << s32 << " for the array size, is too big");
            throw IOException("Unexpectedly large size encountered");
        }

        s32 = (int32_t)size;
        buffer = new byte[s32];
        read(buffer,s32);
    }
    return buffer;
}

int64_t IOBaseStream::readLong_() {
	int64_t l;
	byte buf[Conversions::longSize];
	read(buf, Conversions::longSize);
	Conversions::longFromBytes(l, buf);
	return l;
}

int32_t IOBaseStream::readInt_() {
	int32_t i;
	byte buf[Conversions::intSize];
	read(buf, Conversions::intSize);
	Conversions::intFromBytes(i, buf);
	return i;
}

bool IOBaseStream::readboolean_() {
	byte b = readByte_();
	bool b2(false);
	Conversions::booleanFromBytes(b2, &b);
	return b2;
}

byte IOBaseStream::readByte_() {
	byte buf[1];
	read(buf, 1);
	return buf[0];
}

float IOBaseStream::readFloat_(void) {
	std::string str = readString_();
	const char *text = str.c_str();
	return (float) atof(text);
}

double IOBaseStream::readDouble_(void) {
	std::string str = readString_();
	const char *text = str.c_str();
	return (double) atof(text);
}

std::string IOBaseStream::readString_() {
	int64_t l = readLong_();
	if (l == -1 || l == 0) {
		std::string str("");
		return str;
	}

	if (l < 0 || l > STREAM_MAX_STRING_SIZE) {
		LOG_DEBUG("Got a faulty string size " << l );
		if (l > 0) {

			byte buf[STREAM_MAX_STRING_SIZE];
			buf[STREAM_MAX_STRING_SIZE - 1] = '\0';
			read(buf, (int32_t) STREAM_MAX_STRING_SIZE - 1);
			LOG_DEBUG("String passed was " << buf);
		}
        throw IOException("Unexpectedly large size encountered for string");
	}

	//assert(l > 0);
	//assert(l < STREAM_MAX_STRING_SIZE);
	byte buf[STREAM_MAX_STRING_SIZE];
	buf[l] = '\0';
	buf[0] = '\0';
	read(buf, (int32_t) l);
	std::string str((const char*) buf);
	LOG_DEBUG("reading string from socket: " << str);
	return str;
}



byte *IOBaseStream::readByteArray_(int64_t &size) {
	int64_t l = readLong_();
	if (l == -1 || l == 0) {
		return 0;
	}
	assert(l > 0);
	assert(l < STREAM_MAX_BYTEARRAY_SIZE);
	byte *buf = new byte[(int32_t) l];

	read(buf, (int32_t) l);
	size = l;
	return buf;
}

void IOBaseStream::setCork_() {
	mySink_->setCork();
}

void IOBaseStream::clearCork_() {
	mySink_->clearCork();
}

void IOBaseStream::getCork_() {
    mySink_->getCork();
}

void IOBaseStream::readByteArray_(ByteArray &array) {
	byte *buf = array;
	read(buf, array.size());
}

}

// Local Variables: **
// mode: c++ **
// End: **
