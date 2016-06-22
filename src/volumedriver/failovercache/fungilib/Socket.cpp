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

#include "Socket.h"
#include "Conversions.h"
#include "ByteArray.h"
#include "File.h"

#include "IPv4Socket.h"
#include "IPv6Socket.h"

#include <fcntl.h>
#include <errno.h>
// #ifndef _WIN32
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
// #endif

#include <sys/time.h>
#include <cstring>
#include <cassert>
#include <limits>
#include <sstream>
#include <iostream>
#include <poll.h>

#include <sys/sendfile.h>

#include <youtils/Assert.h>
#include <youtils/IOException.h>
#include <youtils/Logging.h>
#include <youtils/System.h>

#include <rdma/rdma_cma.h>
#include <rdma/rsocket.h>

#include "use_rs.h"

//#undef LOG_DEBUG
//#define LOG_DEBUG LOG_INFO

namespace fungi {


class Interval {
public:
	Interval() {
	}
	void start();
	void stop();
	/**
	 * @brief Returns the number of elapsed seconds rounded to the next higher
	 *        second
	 *
	 * @return the number of elapsed seconds, rounded to the next higher second
	 */
	int64_t ceilSeconds(void) const;
	int64_t seconds() const;
	int64_t microseconds() const;
	double secondsDouble() const;
	int64_t startMicroseconds() const;

private:
	struct timeval start_;
	struct timeval end_;
};

void Interval::start() {
	gettimeofday(&start_, 0);
}

void Interval::stop() {
	gettimeofday(&end_, 0);
}

    int64_t Interval::ceilSeconds(void) const {
        static const int64_t uSecPerSecond = 1000000;

        return (microseconds() + uSecPerSecond - 1) / uSecPerSecond;

    }

int64_t Interval::seconds() const {
	return end_.tv_sec - start_.tv_sec;
}

int64_t Interval::microseconds() const {
	int64_t u = (end_.tv_sec - start_.tv_sec)*(1000*1000);
	u += (end_.tv_usec - start_.tv_usec);
	return u;
}

int64_t Interval::startMicroseconds() const {
	int64_t u = start_.tv_sec*(1000*1000);
	u += start_.tv_usec;
	return u;
}

double Interval::secondsDouble() const {
	const double sec_diff = end_.tv_sec - start_.tv_sec;
	double musec_diff = end_.tv_usec - start_.tv_usec;
	musec_diff += (sec_diff * (1000*1000));
	musec_diff /= (1000*1000);
	return musec_diff;
}

static const int SOCKET_MAX_STRING_SIZE = 1024;
static const int SOCKET_MAX_BYTEARRAY_SIZE = 64 * 1024;

std::unique_ptr<Socket>
Socket::createClientSocket(const std::string &host, uint16_t port, int sock_type)
{
    std::unique_ptr<Socket> sock;
    bool created = false;

    try {
        sock = createSocket(true, sock_type);
        created = true;
        sock->connect(host, port);
    }
    catch (IOException& e1) {
        LOG_DEBUG((created ? "Connect" : "Create") << " RDMA socket [" << host << ":" << port << "] failed (" << getErrorNumber() << ") - retry with TCP");

        created = false;
        try {
            sock = createSocket(false, sock_type);
            created = true;
            sock->connect(host, port);
        }
        catch (IOException& e2) {
            LOG_DEBUG((created ? "Connect" : "Create") << " TCP socket [" << host << ":" << port << "] failed (" << getErrorNumber() << ") - give up");
            throw e2;
        }
    }
    return sock;
}

std::unique_ptr<Socket>
Socket::createSocket(bool rdma, int sock_type) {

    if (rdma)
    { // rsocket cannot handle IPv6
        return std::unique_ptr<Socket>(new IPv4Socket(true, sock_type));
    }
    // always use IPv4 in windows for now TODO
#ifndef _WIN32
    try {
        return std::unique_ptr<Socket>(new IPv6Socket(sock_type));
    } catch (IOException & /* e */) {
          LOG_DEBUG("Fall back on IPv4");
#endif
          return std::unique_ptr<Socket>(new IPv4Socket(false,
                                                        sock_type));
#ifndef _WIN32
    }
#endif
}

Socket::Socket(int domain, int sock_type, bool rdma) :
	sock_(-1), domain_(domain), sock_type_(sock_type), use_rs_(rdma), nonblocking_(false),
    port_(0), stop_(false), connectionInProgress_(false), requestTimeout_(0), remote_ip_(""), remote_port_(0),
    parent_socket_(0), closed_(true), corked_(false) {
	sock_ = rs_socket(domain_, sock_type_, 0);
	if (sock_ == -1) {
		throw IOException("socket", "", getErrorNumber());
	}
	closed_ = false;
	LOG_DEBUG("New unbound socket created:" << sock_ << " transport=" << (use_rs_ ? "RDMA" : "TCP"));
}

Socket::Socket(const Socket *parent_socket, int sock, int domain,
		int sock_type, const char *remote_ip, uint16_t remote_port) :
	sock_(sock),
        domain_(domain),
        sock_type_(sock_type),
        use_rs_(parent_socket->isRdma()),
        nonblocking_(false),
        stop_(false),
        connectionInProgress_(false),
	requestTimeout_(0),
        remote_ip_(remote_ip),
        remote_port_(remote_port),
        parent_socket_(parent_socket),
        closed_(false),
        corked_(false)
{
	LOG_DEBUG("New socket created:" << sock_ << " transport=" << (use_rs_ ? "RDMA" : "TCP"));
}

Socket::~Socket()
{
    closeNoThrow();
}

void Socket::setName(const std::string &name) {
	name_ = name;
}

uint16_t Socket::bound_port() const {
	return port_;
}

const char *Socket::remote_ip() const {
	return remote_ip_.c_str();
}

uint16_t Socket::remote_port() const {
	return remote_port_;
}

void Socket::listen() {
    LOG_DEBUG((use_rs_ ? "RDMA" : "TCP"));
	if (rs_listen(sock_, 5) == -1) {
		throw IOException("listen", "", getErrorNumber());
	}
}

void Socket::setSendbufferssize(int size) {
	if (rs_setsockopt(sock_, SOL_SOCKET, SO_SNDBUF, (char *) &size,
			(int) sizeof(size)) == -1) {
		throw IOException("setsockopt SO_SNDBUF", "", getErrorNumber());
	}
}

int Socket::getSendbufferssize() const {
	int size;
	socklen_t ret_size;
	rs_getsockopt(sock_, SOL_SOCKET, SO_SNDBUF, (char *) &size, &ret_size);
	return size;
}

int Socket::fileno() const {
	return sock_;
}

void Socket::setCork() {
    corked_ = true;
    if (!use_rs_) { // only read-ahead in TCP
        writeInt_(0); // temporary write of length in memory
    }
}

// TODO NoDelay win32 support

void Socket::clearNoDelay() {
#ifndef _WIN32
	u_int yes = 0;
	if (rs_setsockopt(sock_, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) == -1) {
		throw IOException("setsockopt TCP_NODELAY", "", getErrorNumber());
	}
#endif
}

void Socket::setNoDelay() {
#ifndef _WIN32
	u_int yes = 1;
	if (rs_setsockopt(sock_, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) == -1) {
		throw IOException("setsockopt TCP_NODELAY", "", getErrorNumber());
	}
#endif
}

void Socket::clearCork() {
    corked_ = false;
    int32_t size = (int32_t) wbuf_.size();
    if (!use_rs_) { // only read-ahead in TCP
        // Write length-to-come at begin of buffer
//        LOG_INFO("chunk with " << (size - Conversions::intSize) << " bytes follows")
        Conversions::bytesFromInt(wbuf_.data(), (size - Conversions::intSize));
        write(wbuf_.data(), size);
    }
    else {
        write(wbuf_.data(), size);
    }
    wbuf_.clear();
}

void Socket::getCork()
{
    if (!use_rs_) // only read-ahead in TCP
    {
        // Read length of corked chunk
        int32_t corked = readInt_();
//    LOG_INFO("read ahead " << corked << " bytes");
        // Buffer this many bytes
        rbuf_.store(*this, (int) corked);
    }
}

void Socket::setRequestTimeout(double timeout) {
    requestTimeout_ = timeout;
}
void Socket::setNonBlocking() {
  //fuut();
#ifndef _WIN32
	int flags = rs_fcntl2(sock_, F_GETFL);
	if (flags < 0) {
		throw IOException("Socket::nonblocking", "", getErrorNumber());
	}
	flags |= O_NONBLOCK;
	if (rs_fcntl3(sock_, F_SETFL, flags) < 0) {
		throw IOException("Socket::nonblocking2", "", getErrorNumber());
	}
#else
	ULONG on = 1;
	if (ioctlsocket(sock_, FIONBIO, &on) < 0) {
	throw IOException("Socket::nonblocking", "", getErrorNumber());
	}
#endif
	nonblocking_ = true;
}
void Socket::clearNonBlocking()
{
  //fuut();
#ifndef _WIN32
	int flags = rs_fcntl2(sock_, F_GETFL);
	if (flags < 0) {
		throw IOException("Socket::blocking", "", errno);
	}
	flags &= ~O_NONBLOCK;
	if (rs_fcntl3(sock_, F_SETFL, flags) < 0) {
		throw IOException("Socket::blocking2", "", errno);
	}
#else
	ULONG on = 0;
	if (ioctlsocket(sock_, FIONBIO, &on) < 0) {
	throw IOException("Socket::blocking", "", getErrorNumber());
	}
#endif
    nonblocking_ = false;
}

bool Socket::isNonblocking() const {
	return nonblocking_;
}

int Socket::getErrorNumber() {
    //#ifndef _WIN32
	return errno;
        // #else
        //	return WSAGetLastError();
        //#endif
}

int32_t Socket::read(byte *buf, const int32_t n)
{
    // reading from a corked socket is not a good idea...
    assert(!corked_);

    if (!use_rs_) // only read-ahead in TCP
    {
        // Read from buffer if there are data buffered
        if (rbuf_.size() >= (int) n)
        {
            return rbuf_.read(buf, (int) n);
        }
    }

    byte *bufp = buf;
    assert(n < std::numeric_limits<int>::max());
    int32_t n2 = n;
    while (n2 > 0)
    {
        int32_t s = rs_recv(sock_, (char*)bufp, n2, 0);
        assert(s <= n2);
        int err = 0;
        if (s < 0)
        {
            err = getErrorNumber();
            switch (err)
            {
            case EAGAIN:
                if (nonblocking_ || requestTimeout_ > 0)
                {
                    wait_for_read();
                }
                // fall through
            case EINTR:
                continue;
            default:
                LOG_DEBUG("read error < 0, other error");
                throw IOException("Socket::read/other", "", err);
            }
        }

        // s == 0: indicates connection closed, as the socket is
        // readable, and it has to return at least one byte!
        if (s == 0)
        {
            LOG_DEBUG("read error 0, assuming connection closed");
            throw IOException("Socket::read/closed", "", 0);
        }

        n2 -= s;
        bufp += s;
    }

    return n;
}

// remark: on win32 select() only works on sockets, NOT on filedescriptors
// as we only use select() on a single socket here, this is not an issue

void Socket::wait_for_read() {
    //fd_set rfds, efds;
    //	struct timeval tv;
    Interval interval;
    interval.start();

    while (true) {
        //F D_ZERO(&rfds);
        // FD_SET(sock_, &rfds);
        // FD_ZERO(&efds);
        // FD_SET(sock_, &efds);

        /* Wait up to 1 second. */
        //        tv.tv_sec = 1;
        //        tv.tv_usec = 0;
        // remark: we do NOT suffer from the select() signal race
        // condition as we always select() with a timeout

        struct pollfd pfd;

        pfd.fd = sock_;
        pfd.events = POLLIN | POLLPRI;
        pfd.revents = 0;

        int retval = rs_poll(&pfd, 1, 1000);

        //		int retval = select((int)sock_ + 1, &rfds, 0, &efds, &tv);

        const int err = getErrorNumber();

        if (stop_.load()) {
            throw IOException("Socket::wait_for_read", "stop requested", 0);
        }

        if (retval > 0)
        {
            int sockerr = 0;
            socklen_t sockerrlen = sizeof(socklen_t);
            rs_getsockopt(sock_, SOL_SOCKET, SO_ERROR, (char*)&sockerr, &sockerrlen);
            if(sockerr == ECONNREFUSED)
            {
                throw ConnectionRefusedError("Socket::wait_connected/ERROR", 0, sockerr);
            }
            else
            {
                return;
            }
        }

        if (retval < 0 && err != EINTR)
        {
            throw IOException("Socket::wait_for_read", "", err);
        }

        interval.stop();
        if (requestTimeout_ > 0 and
            requestTimeout_ < interval.seconds())
        {
            throw IOException("Socket::wait_for_read", "request timeout", 0);
        }
    }
}

void Socket::wait_for_write() {
    Interval interval;
    interval.start();

	while (true) {
            struct pollfd pfd;
            pfd.fd = sock_;
            pfd.events = POLLOUT | POLLERR | POLLNVAL;
            pfd.revents = 0;

            int retval = rs_poll(&pfd, 1, 1000);

            int err = getErrorNumber();
            if (stop_.load())
            {
                throw IOException("Socket::wait_for_write", "stop requested", 0);
            }

            else if (retval > 0 )
            {
                int sockerr = 0;
                socklen_t sockerrlen = sizeof(socklen_t);

                rs_getsockopt(sock_, SOL_SOCKET, SO_ERROR, (char*)&sockerr, &sockerrlen);
                if(sockerr == ECONNREFUSED)
                {

                    throw ConnectionRefusedError("Socket::wait_connected/ERROR", 0, sockerr);
                }
                else
                {
                    return;
                }
            }

            else if (retval < 0 && err != EINTR)
            {
                throw IOException("Socket::wait_for_write", "", err);
            }
            interval.stop();
            if (requestTimeout_ > 0 && requestTimeout_ < interval.seconds())
            {
                throw IOException("Socket::wait_for_write", "request timeout", 0);
            }
    }
}

int32_t Socket::write(const byte *buf, int32_t n)
{
    if (corked_)
    {
        wbuf_.append((byte *) buf, (int) n);
        return n;
    }

    const byte *bufp = buf;
    assert(n < std::numeric_limits<int>::max());
    int32_t n2 = n;

    while (n2 > 0)
    {
        int32_t s = rs_send(sock_, (const char *)bufp, n2, 0);
        assert(s <= n2);
        int err = 0;
        if (s < 0)
        {
            err = getErrorNumber();
            switch (err)
            {
            case EAGAIN:
                {
                    if (nonblocking_ or requestTimeout_ > 0)
                    {
                        wait_for_write();
                    }
                }
                // fall through
            case EINTR:
                continue;
            default:
                LOG_DEBUG("write error < 0, other error");
                throw IOException("Socket::write", "", err);
            }
        }

        n2 -= s;
        bufp += s;
    }
    return n;
}

void Socket::shutdown()
{
    if (not stop_.exchange(true))
    {
        int ret = rs_shutdown(sock_,
                             SHUT_RDWR);
        if (ret != 0)
        {
            ret = errno;
            LOG_ERROR("Failed to shutdown socket " << name_ << ": " << strerror(ret));
            throw fungi::IOException("Failed to shutdown socket",
                                     name_.c_str(),
                                     ret);
        }
    }
}

const Socket *Socket::parent() const {
	return parent_socket_;
}

void Socket::writeInt_(int32_t i) {
	byte buf[Conversions::intSize];
	Conversions::bytesFromInt(buf, i);
	write(buf, Conversions::intSize);
}

void Socket::writeLong_(int64_t l) {
	byte buf[Conversions::longSize];
	Conversions::bytesFromLong(buf, l);
	write(buf, Conversions::longSize);
}

void Socket::writeboolean_(bool b) {
	byte buf[1];
	buf[0] = b;
	write(buf, 1);
}

void Socket::writeByte_(byte b) {
	write(&b, 1);
}

void Socket::writeString_(const std::string *s) {
	if (s == 0) {
		writeLong_(-1);
	} else {
		writeLong_(s->size());
		if (s->size() > 0) {
			write((const byte *) s->c_str(), s->size());
		}
	}
}

void Socket::writeByteArray_(const ByteArray *array) {
	if (array == 0) {
		writeLong_(-1);
	} else {
		writeLong_(array->size());
		if (array->size() > 0) {
			write(*array, array->size());
		}
	}
}

int64_t Socket::readLong_() {
	int64_t l;
	byte buf[Conversions::longSize];
	read(buf, Conversions::longSize);
	Conversions::longFromBytes(l, buf);
	return l;
}

int32_t Socket::readInt_() {
	int32_t i;
	byte buf[Conversions::intSize];
	read(buf, Conversions::intSize);
	Conversions::intFromBytes(i, buf);
	return i;
}

bool Socket::readboolean_() {
	byte b = readByte_();
	bool b2(false);
	Conversions::booleanFromBytes(b2, &b);
	return b2;
}

byte Socket::readByte_() {
	byte buf[1];
	read(buf, 1);
	return buf[0];
}

std::string Socket::readString_() {
	int64_t l = readLong_();
	if (l == -1 || l == 0) {
		std::string str("");
		return str;
	}
	assert(l> 0);
	assert(l < SOCKET_MAX_STRING_SIZE);
	byte buf[SOCKET_MAX_STRING_SIZE];
	buf[l] = '\0';
	buf[0] = '\0';
	read(buf, (int32_t)l);
	std::string str((const char*) buf);
	return str;
}

byte *Socket::readByteArray_(int64_t &size) {
	int64_t l = readLong_();
	if (l == -1 || l == 0) {
		return 0;
	}
	assert(l> 0);
	assert(l < SOCKET_MAX_BYTEARRAY_SIZE);
	byte *buf = new byte[(int32_t)l];

	read(buf, (int32_t)l);
	size = l;
	return buf;
}

void Socket::readByteArray_(ByteArray &array) {
	byte *buf = array;
	read(buf, array.size());
}

in_addr_t Socket::resolveIPv4(const std::string &addr) {

	struct addrinfo hints;
	struct addrinfo *result;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0; /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	int s = getaddrinfo(addr.c_str(), 0, &hints, &result);
	if (s != 0) {
		std::stringstream ss;
		ss << addr << " : " << gai_strerror(s);
		std::string str = ss.str();
		throw ResolvingError("resolveIPv4: Failure to resolve", str.c_str(), 0);
	}
	if (result == 0) {
		throw ResolvingError("resolveIPv4: Failure to resolve: nothing found",
				addr.c_str(), 0);
	}

	in_addr_t addr2 = ((struct sockaddr_in *) result->ai_addr)->sin_addr.s_addr;
	freeaddrinfo(result);
	return addr2;
}

struct in6_addr Socket::resolveIPv6(const std::string &addr) {

	struct addrinfo hints;
	struct addrinfo *result;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0; /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	hints.ai_flags = AI_V4MAPPED | AI_ALL;

	int s = getaddrinfo(addr.c_str(), 0, &hints, &result);
	if (s != 0) {
		std::stringstream ss;
		ss << addr << " : " << gai_strerror(s);
		std::string str = ss.str();
		throw ResolvingError("resolveIPv6: Failure to resolve", str.c_str(), 0);
	}
	if (result == 0) {
		throw ResolvingError("resolveIPv6: Failure to resolve: nothing found",
				addr.c_str(), 0);
	}
	//Throw an exception instead of failing on assertion. This is necessary for machines
	//with glibc >= 2.9, since the behaviour of getaddrinfo() has changed.
	if(result->ai_family != AF_INET6) {
		throw ResolvingError("resolveIPv6: Failure to resolve, non-IPv6 address", addr.c_str(), 0);
	}
	assert(result->ai_addrlen == sizeof(struct sockaddr_in6));
	struct in6_addr addr2 =
			((struct sockaddr_in6 *) result->ai_addr)->sin6_addr;
	//in_addr_t addr3 = ((struct sockaddr_in *) result->ai_addr)->sin_addr.s_addr;
	//fudeb("XXX %s", Socket::toString(addr3).c_str());
	freeaddrinfo(result);
	return addr2;
}

std::string Socket::toString(const struct in6_addr &addr) {
	//fuut();
#ifndef _WIN32
	char buf[INET6_ADDRSTRLEN + 1];
	if (inet_ntop(AF_INET6, (void*)&addr, buf, INET6_ADDRSTRLEN) == 0) {
		throw IOException("Socket:: failure", "inet_ntop", errno);
	}
	std::string s(buf);
#else
	// TODO: runtime detect XP vs Vista and use inet_ntop on Vista
	// is that possible at all, because the program actually already fails
	// at startup on windows XP if a call to inet_ntop is contained in the
	// binary...
	std::string s("inet_ntop missing in win32 < vista");
#endif
	return s;
}

std::string Socket::toString(const in_addr_t &addr) {
	//fuut();
#ifndef _WIN32
	char buf[INET_ADDRSTRLEN + 1];
	if (inet_ntop(AF_INET, (void*)&addr, buf, INET_ADDRSTRLEN) == 0) {
		throw IOException("Socket:: failure", "inet_ntop", getErrorNumber());
	}
	std::string s(buf);
#else
	// same remark as above for toString
	std::string s("inet_ntop missing in win32 < vista");
#endif
	return s;
}

void Socket::close() {
	if (closed_) {
		LOG_DEBUG("Socket already closed: " << sock_);
		return;
	}
#ifndef _WIN32
	if (rs_close(sock_) < 0) {
		throw IOException("Failure to close socket", "", getErrorNumber());
	}
#else
	// on windows, close() is not allowed on sockets
	if (::closesocket(sock_) < 0) {
		throw IOException("Failure to close socket", "", getErrorNumber());
	}
#endif
	LOG_DEBUG("closed:" << sock_);
	closed_ = true;
	sock_ = -1;
}
void Socket::closeNoThrow(){
	if (closed_) {
		LOG_DEBUG("Socket already closed: " << sock_);
		return;
	}
#ifndef _WIN32
	if (rs_close(sock_) < 0) {
		int e = getErrorNumber();
		if(e == EBADF){
		    //if it is a bad file descriptor, due to socket already closed, don't log anything.
		}else{
			char *res = strerror(e);
			LOG_WARN("Failure to close socket("<< sock_<<") "<< e << "(" << (res!=0?res:"null") << ") ... ignoring");
		}
	}
#else
	if (::closesocket(sock_) < 0) {
		int e = getErrorNumber();
		if(e == EBADF){
			// TODO: is this the same on windows ??? probably not (unverified)
		} else{
			LOG_WARN("Failure to close socket " << e <<" ... ignoring");
		}
	}
#endif
	LOG_DEBUG("closed:" << sock_);
	closed_ = true;
	sock_ = -1;
}

// for now only do sendfile on linux...
#ifdef __linux__
void Socket::send(File &file, int32_t count, int32_t offset) {
	//fudeb("Socket::send %s(%d) to %d.%d.%d.%d:%d", file.name().c_str(), count, NIPQUAD(remote_ip_), remote_port_);
	int res = 0;
	const bool nb = isNonblocking();
	if (nb) clearNonBlocking();
	off_t off = offset;
	off_t *offp = &off;
	do {
#ifdef DARWIN
		off_t size_as_off_t = count;
		res = sendfile(file.fileno(),fileno(), off, &size_as_off_t, 0, 0);
#else
		res = sendfile(fileno(), file.fileno(), offp, count);
#endif
		if (res > 0) {
			count -= res;
		}
		// next line might be problematic on solaris and on linux it seems we always do the
		// sendfile in one go..
		offp = 0; // do not do any more seeking, just continue at current offset
	} while (res == -1 && errno == EINTR);
	if (nb) setNonBlocking();
	if (res == -1) {
		throw IOException("Socket::send", file.name().c_str(), getErrorNumber());
	}
	//fudeb("sendfile res: %d", res);
}
#else

// for now we do NOT want to use TransmitFile in windows, because it kills
// performance on non-server editions of windows, because only a very few of them (1 or 2)
// are allowed to run in parallel...

//#define WIN_USE_TRANSMITFILE
#undef WIN_USE_TRANSMITFILE
#ifdef WIN_USE_TRANSMITFILE
void Socket::send(File &file, int32_t count) {
	file.rewind();
	const bool nb = isNonblocking();
	if (nb) clearNonblocking();

	static GUID TransmitFileGUID = WSAID_TRANSMITFILE;
	LPFN_TRANSMITFILE TransmitFile;
	DWORD cbBytesReturned;

	if (WSAIoctl(fileno(),
               SIO_GET_EXTENSION_FUNCTION_POINTER,
               &TransmitFileGUID,
               sizeof(TransmitFileGUID),
               &TransmitFile,
               sizeof(TransmitFile),
               &cbBytesReturned,
               0,
               0) == SOCKET_ERROR) {
				   throw IOException("Socket::send: WSAIoctl failure");
	}
	HANDLE h = (HANDLE)_get_osfhandle(file.fileno());

	BOOL res = TransmitFile(
		/* SOCKET */ fileno(),
		/* HANDLE hFile */ h,
		/* DWORD nNumberOfBytesToWrite */ (DWORD)count,
		/* DWORD nNumberOfBytesPerSend */ 0  /* use default */,
		/* LPOVERLAPPED lpOverlapped */ 0 /* don't do overlapped => synchronous call */,
		/* LPTRANSMIT_FILE_BUFFERS lpTransmitBuffers */ 0 /* no header or tail */,
		/* DWORD dwFlags */ 0
	);
	int err = 0;
	if (res == FALSE) {
		err = getErrorNumber();
	}
	if (nb) setNonblocking();
	if (res == false) {
		throw IOException("Socket::send[TransmitFile]", file.name().c_str(), err);
	}
}
#else
void Socket::send(File &file, int32_t count, int32_t offset) {
	if(offset)
		file.seek(offset);
	byte *buf = new byte[count];
    file.append(buf, count);
    write(buf, count);
    delete[] buf;
}
#endif
#endif

bool Socket::isIpv4() const {
	return domain_ == AF_INET;
}

bool Socket::isIpv6() const {
	return domain_ == AF_INET6;
}

int Socket::getDomain() const {
	return domain_;
}

bool Socket::isRdma() const {
    return use_rs_;
}

bool Socket::wait_connected(double seconds)
{
    // struct timeval tv;
    // tv.tv_sec = (long)seconds;
    // tv.tv_usec = 0;
    // fd_set rset, wset;
    // FD_ZERO(&rset);
    // FD_SET(sock_, &rset);
    // wset = rset;
    struct pollfd pfd;
    pfd.fd = sock_;
    // boost asio only wait for pollout
    pfd.events = POLLOUT ;
    pfd.revents = 0;

    while(true)
    {

        int retval = rs_poll(&pfd, 1, seconds);

        if (retval < 0)
        {
            const int err = getErrorNumber();
            if (err == EINTR)
            {
                continue;
            }
            rs_close(sock_);

            closed_ = true;
            throw IOException("Socket::wait_connected/poll", name_.c_str(), err);
        }
        else if (retval > 0)
        {
            int val = get_SO_ERROR();
            if (val != 0)
            {
                rs_close(sock_);
                closed_ = true;
                throw ConnectionRefusedError("Socket::wait_connected/ERROR", name_.c_str(), val);
            }
            // socket got connected
            connectionInProgress_ = false;
            clearNonBlocking();
            return true;
        }
        else /* res == 0 */
        {
            // timeout
            return false;
        }
    }
}

int Socket::get_SO_ERROR()
{
    socklen_t sl = sizeof(int);
    int val = 0;
    if (rs_getsockopt(sock_, SOL_SOCKET, SO_ERROR, (
#ifdef WIN32
		char*
#else
		void*
#endif
		)(&val), &sl) < 0) {
        throw IOException("Socket::get_SO_ERROR", name_.c_str(), getErrorNumber());
    }
    return val;
}

}


// Local Variables: **
// mode: c++ **
// End: **
