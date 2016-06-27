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

#ifndef _SOCKET_H
#define	_SOCKET_H

#include "ByteArray.h"
#include "Streamable.h"
#include "Buffer.h"

#include <atomic>

#include <string>
#include <cassert>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>

#include <boost/optional.hpp>

#include <youtils/Logging.h>

namespace fungi {

class File;
class Socket;

class Socket
    : public Streamable
{
public:
    static std::unique_ptr<Socket>
    createClientSocket(const std::string &host,
                       uint16_t port,
                       int sock_type = SOCK_STREAM);

    static std::unique_ptr<Socket>
    createSocket(bool rdma,
                 int sock_type = SOCK_STREAM);

    virtual ~Socket();
    // these are specific for IPv4 or IPv6

    void setName(const std::string &name);
    virtual const std::string toString() const = 0;

    /** @exception IOException */
    virtual void bind(const boost::optional<std::string>& addr,
                      uint16_t port,
                      bool reuseaddr = true) = 0;

    /** @exception IOException */
    virtual Socket *accept(bool nonblocking = true)=0;

    /** @exception IOException */
    virtual void connect(const std::string &host, uint16_t port) = 0;

    /** @exception IOException
     * will return false if connection is still pending,
     * true if connection succeeded immediately
     * if the connection is pending, use wait_connected to
     * wait a specific time for the connection to become active
     */
    virtual bool connect_nb(const std::string &host, uint16_t port) = 0;
    /** @exception IOException
     * only useful used after connect_nb returned false
     * returns true when connection succeeded; false
     * when it is still pending or throws when it gets in an
     * error situation
     */
    bool wait_connected(double seconds);

    /** @exception IOException */
    void listen();

    /** @exception IOException */
    void send(File &file, int32_t count, int32_t offset = 0);

    // add splice() syscall ?

    int fileno() const;
    uint16_t bound_port() const;

    bool isIpv4() const;
    bool isIpv6() const;
    int getDomain() const;
    bool isRdma() const;

    uint16_t remote_port() const;
    const char *remote_ip() const;

    /** @exception IOException */
    int32_t read(byte *buf,
                 const int32_t n);

    /** @exception IOException */
    int32_t write(const byte *buf, int32_t n);

    /** @exception IOException */
    void setCork();

    /** @exception IOException */
    void clearCork();

    /** @exception IOException */
    void getCork();

    void setRequestTimeout(double timeout);

    /** @exception IOException */
    void setNonBlocking();

    /** @exception IOException */
    void clearNonBlocking();

    bool isNonblocking() const;

    void shutdown();

    /** @exception IOException */
    void setSendbufferssize(int size);

    /** @exception IOException */
    int getSendbufferssize() const;

    const Socket *parent() const;

    virtual bool isClosed() const { return closed_; }

    /** @exception IOException */
    void setNoDelay();
    /** @exception IOException */
    void clearNoDelay();

public:
    static struct in6_addr resolveIPv6(const std::string &addr);
    static in_addr_t resolveIPv4(const std::string &addr);

    static std::string toString(const struct in6_addr &addr);
    static std::string toString(const in_addr_t &addr);


    //#define USE_SOCKETOPS
    //#include "SocketOps.h"
    //#undef USE_SOCKETOPS

protected:
    Socket(int domain, int sock_type, bool rdma);
    Socket(const Socket *parent_sock, int sock, int domain, int sock_type, const char *remote_ip,
           uint16_t remote_port);

    static int getErrorNumber();

    int sock_;
    int domain_;
    int sock_type_;
    bool use_rs_;
    bool nonblocking_;
    boost::optional<std::string> addr_;
    uint16_t port_;
    void wait_for_read();
    void wait_for_write();
    std::atomic<bool> stop_;
    std::string name_;
    bool connectionInProgress_;
    double requestTimeout_;

    int get_SO_ERROR();

    /** @exception IOException */
    void close();
    void closeNoThrow();

private:
    DECLARE_LOGGER("Socket");

    void writeInt_(int32_t i);
    void writeLong_(int64_t l);
    void writeboolean_(bool b);
    void writeByte_(byte b);
    void writeString_(const std::string *s);
    void writeByteArray_(const ByteArray *array);

    int64_t readLong_();
    int32_t readInt_();
    bool readboolean_();
    std::string readString_();
    byte *readByteArray_(int64_t &size);
    void readByteArray_(ByteArray &array);
    byte readByte_();

    const std::string remote_ip_;
    uint16_t remote_port_;
    const Socket *parent_socket_;
    bool closed_;
    bool corked_;

    Socket(const Socket &);
    Socket & operator=(const Socket &);

    Buffer wbuf_;
    Buffer rbuf_;
};

}

#endif	/* _SOCKET_H */
