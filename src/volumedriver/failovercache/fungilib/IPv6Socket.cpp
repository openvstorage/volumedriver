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

#include "IPv6Socket.h"

#include <errno.h>
#include <cstring>
// #ifndef _WIN32
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
// #include <netinet/in.h>
// #endif
#include <fcntl.h>
#include <cassert>

#include <sstream>
// #ifdef DARWIN
//   #include <socket.h>
// #else
// #ifndef _WIN32
  #include <sys/sendfile.h>
//#endif
// #endif

//#undef LOG_DEBUG
//#define LOG_DEBUG LOG_INFO

#include <boost/optional/optional_io.hpp>

#include <youtils/IOException.h>
#include <youtils/Logging.h>

namespace fungi {

using namespace std::literals::string_literals;

IPv6Socket::IPv6Socket(int sock_type) :
	Socket(AF_INET6, sock_type, false) {
}

IPv6Socket::IPv6Socket(const Socket *parent_socket, int asock, int sock_type_, const char *remote_ip,
		uint16_t remote_port) :
	Socket(parent_socket, asock, AF_INET6, sock_type_, remote_ip, remote_port) {

}

const std::string
IPv6Socket::toString() const
{
	std::stringstream ss;
	ss << "IPv6Socket("
           << "name = " << name_
           << " addr = " << addr_
           << " port = " << port_
           << " sock = " << sock_
           << ")";
	std::string s = ss.str();
	return s;
}

void IPv6Socket::bind(const boost::optional<std::string>& addr,
                      uint16_t port,
                      bool reuseaddr)
{
    in6_addr ia = in6addr_any;
    if (addr)
    {
        int ret = 0;

        if (addr->find(":") == std::string::npos)
        {
            const std::string a("::FFFF:"s + *addr);
            ret = ::inet_pton(AF_INET6,
                              a.c_str(),
                              &ia);
        }
        else
        {
            ret = ::inet_pton(AF_INET6,
                              addr->c_str(),
                              &ia);
        }

        if (ret == 0)
        {
            LOG_ERROR(*addr << " could not be converted to an IPv6 address");
            throw fungi::IOException("Could not convert to IPv6 address",
                                     addr->c_str());
        }
    }

    struct sockaddr_in6 inaddr6;
    struct sockaddr *sa = 0;
    socklen_t slen = 0;

    LOG_DEBUG("bind ipv6 "<< sock_<< " to addr: " << addr <<
              ", port: "<< port << "/" << (use_rs_ ? "RDMA" : "TCP"));

    sa = (struct sockaddr *) &inaddr6;
    memset(&inaddr6, 0, sizeof(struct sockaddr_in6));
    slen = sizeof(inaddr6);
    inaddr6.sin6_family = AF_INET6;
    inaddr6.sin6_port = htons(port);
    inaddr6.sin6_addr = ia;

    if (reuseaddr)
    {
        u_int yes = 1;
        if (setsockopt(sock_,
                       SOL_SOCKET,
                       SO_REUSEADDR,
                       (const char*)&yes, sizeof(yes)) == -1)
        {
            throw IOException("setsockopt", "", getErrorNumber());
        }
    }

    if (::bind(sock_, sa, slen) == -1)
    {
        std::stringstream ss;
        ss << "bind to port " << port;
        std::string s = ss.str();
        throw IOException(s.c_str(), "", getErrorNumber());
    }

    port_ = port;
}

Socket *IPv6Socket::accept(bool nonblocking) {
    LOG_DEBUG((use_rs_ ? "RDMA" : "TCP"));
	int asock = 0;
	while (!stop_) {
		if (nonblocking_) {
			//fudeb("Nonblocking accept");
			wait_for_read();
		} else {
			//fudeb("Blocking accept");
		}
		asock = ::accept(sock_, 0, 0);
		if (asock > 0)
			break;
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
	char str[INET6_ADDRSTRLEN];

	struct sockaddr_in6 clientaddr;
	socklen_t addrlen = sizeof(clientaddr);
	int res = getpeername(asock, (struct sockaddr *) &clientaddr, &addrlen);
	if (res < 0) {
		throw IOException("Socket::accept() Failed to find peer IP/port", "",
				res);
	}
#ifndef _WIN32
	if (inet_ntop(AF_INET6, &clientaddr.sin6_addr, str, sizeof(str))) {
		LOG_DEBUG("IPv6 Client address is " << str);
	} else {
		throw IOException(
				"Socket::accept() Failure to resolve remote peer for accepted connection");
	}
	// TODO: also do some form of resolving for win32
#endif
	LOG_DEBUG("Client port is " << ntohs(clientaddr.sin6_port));

	remote_port = ntohs(clientaddr.sin6_port);

	Socket *asock2 = new IPv6Socket(this, asock, sock_type_, str, remote_port);
	if (nonblocking) {
		asock2->setNonBlocking();
	}
	// TODO make sendbuffersize
	// asock2->setSendbufferssize(32 * 1024);
	return asock2;
}

static void setup_sockaddr_v6_(struct sockaddr_in6 &inaddr6, const std::string &host, uint16_t port)
{
    memset(&inaddr6, 0, sizeof(inaddr6));
	inaddr6.sin6_family = AF_INET6;
	inaddr6.sin6_port = htons(port);
	inaddr6.sin6_addr = Socket::resolveIPv6(host);
}

// TODO: avoid code duplication between ipv4 and ipv6 connect?
void IPv6Socket::connect(const std::string &host, uint16_t port) {
    LOG_DEBUG("IPv6Socket::connect "<< host.c_str() << ":" << port << "/" << (use_rs_ ? "RDMA" : "TCP"));
	struct sockaddr *saddr = 0;
	struct sockaddr_in6 inaddr6;
    setup_sockaddr_v6_(inaddr6, host, port);
	socklen_t len = sizeof(sockaddr_in6);
	saddr = (struct sockaddr *) &inaddr6;

	if (::connect(sock_, saddr, len) < 0) {
		std::stringstream ss;
		ss << host << ":" << port;
		std::string str = ss.str();
		throw ConnectionRefusedError("Failure to connect to", str.c_str(), getErrorNumber());
	};
}

bool IPv6Socket::connect_nb(const std::string &host, uint16_t port) {
	LOG_DEBUG("IPv6Socket::connect_nb "<< host.c_str() <<":" << port);
    connectionInProgress_ = true;
	struct sockaddr *saddr = 0;
	struct sockaddr_in6 inaddr6;
    setup_sockaddr_v6_(inaddr6, host, port);
	socklen_t len = sizeof(sockaddr_in6);
	saddr = (struct sockaddr *) &inaddr6;
    setNonBlocking();
    int res = ::connect(sock_, saddr, len);
    if (res == 0)
    {
        connectionInProgress_ = false;
        clearNonBlocking();
        return true;
    }
    const int err = getErrorNumber();
#ifdef WIN32
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
