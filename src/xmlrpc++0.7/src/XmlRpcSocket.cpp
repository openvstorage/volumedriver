
#include "XmlRpcSocket.h"
#include "XmlRpcUtil.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>

#include <youtils/ScopeExit.h>
#include <youtils/System.h>

using namespace XmlRpc;

namespace yt = youtils;

namespace
{

void
enable_keepalive(int fd)
{
    static const int keep_cnt =
        yt::System::get_env_with_default("XMLRPC_KEEPCNT",
                                         5);

    static const int keep_idle =
        yt::System::get_env_with_default("XMLRPC_KEEPIDLE",
                                         60);

    static const int keep_intvl =
        yt::System::get_env_with_default("XMLRPC_KEEPINTVL",
                                         60);

    yt::System::setup_tcp_keepalive(fd,
                                    keep_cnt,
                                    keep_idle,
                                    keep_intvl);
}

}

// These errors are not considered fatal for an IO operation; the operation will be re-tried.
static inline bool
nonFatalError()
{
  int err = XmlRpcSocket::getError();
  return (err == EINPROGRESS || err == EAGAIN || err == EWOULDBLOCK || err == EINTR);
}


int
XmlRpcSocket::socket()
{
  return (int) ::socket(AF_INET, SOCK_STREAM, 0);
}


void
XmlRpcSocket::close(int fd)
{
    LOG_DEBUG("fd " << fd);
    ::close(fd);
}

bool
XmlRpcSocket::setNonBlocking(int fd)
{
  return (fcntl(fd, F_SETFL, O_NONBLOCK) == 0);
}


bool
XmlRpcSocket::setReuseAddr(int fd)
{
  // Allow this port to be re-bound immediately so server re-starts are not delayed
  int sflag = 1;
  return (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&sflag, sizeof(sflag)) == 0);
}

// Bind to a specified port
bool
XmlRpcSocket::bind(const boost::optional<std::string>& addr,
                   int fd,
                   int port)
{
    in_addr ia{ htonl(INADDR_ANY) };

    if (addr)
    {
        int ret = ::inet_aton(addr->c_str(),
                              &ia);
        if (ret == 0)
        {
            return false;
        }
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr = ia;
    saddr.sin_port = htons(port);
    return (::bind(fd, (struct sockaddr *)&saddr, sizeof(saddr)) == 0);
}

// Set socket in listen mode
bool
XmlRpcSocket::listen(int fd, int backlog)
{
  return (::listen(fd, backlog) == 0);
}

int
XmlRpcSocket::accept(int fd)
{
  struct sockaddr_in addr;
#if defined(_WINDOWS)
  int
#else
  socklen_t
#endif
    addrlen = sizeof(addr);

  int res = ::accept(fd, (struct sockaddr*)&addr, &addrlen);
  if (res >= 0)
  {
     try
     {
         enable_keepalive(res);
     }
     catch (...)
     {
         ::close(res);
         res = -1;
     }
  }

  return res;
}

// Connect a socket to a server (from a client)
bool
XmlRpcSocket::connect(int fd, std::string& host, int port)
{
  try
  {
     enable_keepalive(fd);
  }
  catch (...)
  {
      return -1;
  }

  addrinfo hints;
  memset(&hints, 0x0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;

  addrinfo* infos = nullptr;
  const std::string portstr(boost::lexical_cast<std::string>(port));
  int result = getaddrinfo(host.c_str(),
                           portstr.c_str(),
                           &hints,
                           &infos);
  if (result != 0)
  {
      LOG_ERROR("Failed to resolve " << host << ":" << port <<
                ": " << gai_strerror(result));
      return false;
  }

  auto on_exit(yt::make_scope_exit([&]
                                   {
                                       freeaddrinfo(infos);
                                   }));

  for (addrinfo* ai = infos; ai != nullptr; ai = ai->ai_next)
  {
      result = ::connect(fd,
                         ai->ai_addr,
                         ai->ai_addrlen);
      // For asynch operation, this will return EWOULDBLOCK (windows) or
      // EINPROGRESS (linux) and we just need to wait for the socket to be writable...
      if (result == 0 or nonFatalError())
      {
          return true;
      }
  }

  LOG_ERROR("Failed to connect to " << host << ":" << port);

  return false;
}

// Read available text from the specified socket. Returns false on error.
bool
XmlRpcSocket::nbRead(int fd, std::string& s, bool *eof)
{
  const int READ_SIZE = 4096;   // Number of bytes to attempt to read at a time
  char readBuf[READ_SIZE];

  bool wouldBlock = false;
  *eof = false;

  while ( ! wouldBlock && ! *eof) {
    int n = read(fd, readBuf, READ_SIZE-1);
    LOG_DEBUG("read/recv returned " <<  n);

    if (n > 0) {
      readBuf[n] = 0;
      s.append(readBuf, n);
    } else if (n == 0) {
      *eof = true;
    } else if (nonFatalError()) {
      wouldBlock = true;
    } else {
      return false;   // Error
    }
  }
  return true;
}


// Write text to the specified socket. Returns false on error.
bool
XmlRpcSocket::nbWrite(int fd, std::string& s, int *bytesSoFar)
{
  int nToWrite = int(s.length()) - *bytesSoFar;
  char *sp = const_cast<char*>(s.c_str()) + *bytesSoFar;
  bool wouldBlock = false;

  while ( nToWrite > 0
          // && ! wouldBlock
          ) {
    int n = write(fd, sp, nToWrite);
    LOG_DEBUG("send/write returned " << n);

    if (n > 0) {
      sp += n;
      *bytesSoFar += n;
      nToWrite -= n;
    } else if (nonFatalError()) {
      wouldBlock = true;
    } else {
      return false;   // Error
    }
  }

  (void) wouldBlock;
  return true;
}

// Returns last errno
int
XmlRpcSocket::getError()
{
  return errno;
}

// Returns message corresponding to last errno
std::string
XmlRpcSocket::getErrorMsg()
{
  return getErrorMsg(getError());
}

// Returns message corresponding to errno... well, it should anyway
std::string
XmlRpcSocket::getErrorMsg(int error)
{
  char err[60];
  snprintf(err,sizeof(err),"error %d", error);
  return std::string(err);
}

// Local Variables: **
// mode: c++ **
// End: **
