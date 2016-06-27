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

#ifndef CUSTOMIOSTREAM_H_
#define CUSTOMIOSTREAM_H_

#include "ByteArray.h"
#include "Streamable.h"
#include "Socket.h"

#include <cassert>

namespace fungi
{
// Will need at least
// 1) flushing support
// 2) check if you wanna read n bytes from a sink you actually get them

class IOBaseStream
{
public:
    IOBaseStream(Socket& sock)
        : mySink_(sock)
        , rdma_(sock.isRdma())
    {}

    explicit IOBaseStream(Streamable& stream)
        : mySink_(stream)
        , rdma_(false)
    {}

    ~IOBaseStream() = default;

    bool isRdma() const
    {
        return rdma_;
    }

    /** @exception IOException */
    int32_t read(byte *buf, int32_t n)
    {
        return mySink_.read(buf, n);
    }

    /** @exception IOException */
    int32_t write(const byte *buf, int32_t n) {
        return mySink_.write(buf, n);
    }

    void close() {
        mySink_.close();
    }

    void closeNoThrow() {
        mySink_.closeNoThrow();
    }
    Streamable&
    getSink()
    {
        return mySink_;
    }

    class Null {};
    class Cork {};
    class Uncork {};
    class RequestTimeout {
    public:
        RequestTimeout(double requestTimeout) : requestTimeout_(requestTimeout) {}

        double getRequestTimeout() const { return requestTimeout_; }
    private:
        double requestTimeout_;
    };
    static Cork cork;
    static Uncork uncork;

    template<typename T>
    IOBaseStream& operator<<(T* /* val */)
    {
        // Y42 wants this to break at compile time instead of
        // e.g. streaming a bool at run time
        // Oh yeah, static_assert(false) makes clang unhappy, hence this weird stuff.
        static_assert(T::dummy == false or T::dummy == true,
                      "write a specialization");
        return *this;
    }

    IOBaseStream &operator<<(byte i);
    IOBaseStream &operator<<(bool i);
    IOBaseStream &operator<<(int32_t i);
    IOBaseStream& operator<<(uint32_t);
    IOBaseStream& operator<<(uint16_t);

    IOBaseStream& operator<<(uint64_t i);
    IOBaseStream &operator<<(int64_t i);
    IOBaseStream& operator <<(const float number);
    IOBaseStream& operator <<(const double number);
    IOBaseStream &operator<<(const std::string &i);
    IOBaseStream &operator<<(const ByteArray &i);
    IOBaseStream &operator<<(const IOBaseStream::Null &);
    IOBaseStream &operator<<(const Cork &);
    IOBaseStream &operator<<(const Uncork &);
    IOBaseStream &operator<<(const RequestTimeout&);
    IOBaseStream &operator>>(int32_t &i);
    IOBaseStream &operator>>(uint32_t &i);
    IOBaseStream &operator>>(int64_t &i);
    IOBaseStream &operator>>(uint64_t &i);
    IOBaseStream &operator>>(bool &i);

    IOBaseStream& operator>>(uint16_t&);
    IOBaseStream &operator>>(byte &i);
    IOBaseStream& operator >>(float& number);
    IOBaseStream& operator >>(double& number);
    IOBaseStream &operator>>(std::string &i);
    byte* readByteArray(int32_t &size);
    void readIntoByteArray(byte* buf, uint32_t size);
    IOBaseStream &operator>>(const Cork &);

private:
    DECLARE_LOGGER("IOBaseStream");

    Streamable& mySink_;
    bool rdma_;

    void writeInt_(int32_t i);
    void writeLong_(int64_t l);
    void writeboolean_(bool b);
    void writeByte_(byte b);

    void writeFloat_(float number);
    void writeDouble_(double number);
    void writeString_(const std::string *s);
    void setCork_();
    void clearCork_();
    void writeByteArray_(const ByteArray *array);
    int64_t readLong_();
    int32_t readInt_();
    bool readboolean_();
    byte readByte_();
    float readFloat_(void);
    double readDouble_(void);
    std::string readString_();
    byte *readByteArray_(int64_t &size);
    void readByteArray_(ByteArray &array);
    void getCork_();
};
}

#endif /* CUSTOMIOSTREAM_H_ */
