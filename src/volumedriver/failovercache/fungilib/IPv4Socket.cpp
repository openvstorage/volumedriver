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

#include "IPv4Socket.h"

#include <errno.h>
#include <cstring>
// #ifndef _WIN32
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
// #endif

#include <fcntl.h>
#include <cassert>
#include <sstream>
#include <iostream>

// #ifdef DARWIN
//   #include <socket.h>
// #else

//#ifndef _WIN32
#include <sys/sendfile.h>
//#endif
// #endif

#include <rdma/rdma_cma.h>
#include <rdma/rsocket.h>

#include "use_rs.h"

#include <boost/algorithm/string.hpp>
#include <boost/optional/optional_io.hpp>

#include <youtils/IOException.h>
#include <youtils/Logging.h>

//#undef LOG_DEBUG
//#define LOG_DEBUG LOG_INFO

namespace fungi {

namespace bc = boost::chrono;

IPv4Socket::IPv4Socket(bool rdma, int sock_type):Socket(AF_INET, sock_type, rdma) {
}

void
IPv4Socket::bind(const boost::optional<std::string>& addr,
                 uint16_t port,
                 bool reuseaddr)
{
    in_addr ia{ htonl(INADDR_ANY) };

    if (addr)
    {
        int ret = ::inet_aton(addr->c_str(),
                              &ia);
        if (ret == 0)
        {
            throw IOException("Failed to convert to IP address",
                              addr->c_str());
        }
    }

    struct sockaddr_in inaddr;
    struct sockaddr *sa = 0;
    socklen_t slen = 0;

    LOG_DEBUG("bind ipv4 "<< sock_<<" to port: "<< port << "/" << (use_rs_ ? "RDMA" : "TCP"));
    sa = (struct sockaddr *) &inaddr;
    memset(&inaddr, 0, sizeof(struct sockaddr_in));
    slen = sizeof(inaddr);
    inaddr.sin_port = htons(port);
    inaddr.sin_family = AF_INET;
    inaddr.sin_addr = ia;

    if (reuseaddr)
    {
        u_int yes = 1;
        if (rs_setsockopt(sock_,
                          SOL_SOCKET,
                          SO_REUSEADDR,
                          (const char*)&yes,
                          sizeof(yes)) == -1)
        {
            throw IOException("setsockopt", "", getErrorNumber());
        }
    }

    if (rs_bind(sock_, sa, slen) == -1)
    {
        std::stringstream ss;
        ss << "bind to port " << port;
        std::string s = ss.str();
        throw IOException(s.c_str(), "", getErrorNumber());
    }
    addr_ = addr;
    port_ = port;
}

const std::string
IPv4Socket::toString() const
{
	std::stringstream ss;
	ss << "IPv4Socket("
           << "name = " << name_
           << " addr = " << addr_
           << " port = " << port_
           << " sock = " << sock_
           << ")";
	std::string s = ss.str();
	return s;
}

Socket *IPv4Socket::accept(bool nonblocking) {
	//fudeb("Socket::accept(s:%d)", sock_);
    LOG_DEBUG((use_rs_ ? "RDMA" : "TCP"));
	int asock = 0;
	while (!stop_) {
		if (nonblocking_) {
			wait_for_read();
		}
		asock = rs_accept(sock_, 0, 0);
		if (asock >= 0)
                    break;
                LOG_WARN("accept encountered " << strerror(errno));
		if (asock < 0 && errno != EAGAIN && errno != EINTR
#ifndef _WIN32
                    && errno != ECONNABORTED && errno != EPROTO
#endif
                    ) {
                    throw IOException("accept", "", getErrorNumber());
		}

	}
	if (stop_) {
		return 0;
	}

	uint16_t remote_port = 0;
	char str[INET_ADDRSTRLEN];

	struct sockaddr_in clientaddr;
	socklen_t addrlen = sizeof(clientaddr);
	int res = rs_getpeername(asock, (struct sockaddr *) &clientaddr, &addrlen);
	if (res < 0) {
		throw IOException("Socket::accept() Failed to find peer IP/port", "",
				res);
	}
	char * buf = inet_ntoa(clientaddr.sin_addr);
	if (buf != 0) {
		str[0] = 0;
#pragma warning( push )
#pragma warning( disable: 4996) // we use strncpy in a safe way
		strncpy(str, buf, INET_ADDRSTRLEN);
#pragma warning( pop )
		LOG_DEBUG("IPv4 Client address is " << str);
		LOG_DEBUG("Client port is " << ntohs(clientaddr.sin_port));
	} else {
		throw IOException(
				"Socket::accept() Failure to resolve remote peer for accepted connection");
	}

	remote_port = ntohs(clientaddr.sin_port);

	Socket *asock2 = new IPv4Socket(this, asock, sock_type_, str, remote_port);
	if (nonblocking) {
		asock2->setNonBlocking();
	}
	// TODO make sendbuffersize
	// asock2->setSendbufferssize(32 * 1024);
	return asock2;
}

IPv4Socket::IPv4Socket(const Socket *parent_socket, int asock, int sock_type_, const char *remote_ip, uint16_t remote_port)
:Socket(parent_socket, asock, AF_INET, sock_type_, remote_ip, remote_port){

}

static void setup_sockaddr_v4_(struct sockaddr_in &inaddr, const std::string &host, uint16_t port)
{
    memset(&inaddr, 0, sizeof(inaddr));
	inaddr.sin_family = AF_INET;
	inaddr.sin_port = htons(port);
	inaddr.sin_addr.s_addr = Socket::resolveIPv4(host);
}

int
IPv4Socket::poll_(pollfd* pfd,
                  nfds_t nfd,
                  const boost::optional<bc::milliseconds>& timeout)
{
    const int t = timeout ? timeout->count() : -1;
    return rs_poll(pfd, nfd, t);
}

// nonblocking connect is described in detail in chapter 15.4 of stevens unix network programming
bool IPv4Socket::connect_nb(const std::string &host, uint16_t port)
{
	LOG_DEBUG("IPv4Socket::connect_nb "<< host.c_str() << ":" << port);
    connectionInProgress_ = true;
	struct sockaddr *saddr = 0;
	struct sockaddr_in inaddr;
    setup_sockaddr_v4_(inaddr, host, port);
	socklen_t len = sizeof(sockaddr_in);
	saddr = (struct sockaddr *) &inaddr;
    setNonBlocking();
    int res = rs_connect(sock_, saddr, len);
    if (res == 0)
    {
        connectionInProgress_ = false;
        clearNonBlocking();
        return true;
    }
    const int err = getErrorNumber();
#ifdef WIN32
	if (err == WSAEWOULDBLOCK)
		return true;

	if (err != WSAEINPROGRESS && err != WSAEINTR) {
#else
	if (err != EINPROGRESS && err != EINTR) {
#endif
		std::stringstream ss;
		ss << host << ":" << port;
		std::string str = ss.str();
		throw ConnectionRefusedError("Failure to connect to", str.c_str(), err);
	};
    return false;
}


}
