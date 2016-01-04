
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

  struct sockaddr_in saddr;
  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;

  struct hostent *hp = gethostbyname(host.c_str());
  if (hp == 0) return false;

  saddr.sin_family = hp->h_addrtype;
  memcpy(&saddr.sin_addr, hp->h_addr, hp->h_length);
  saddr.sin_port = htons((u_short) port);

  // For asynch operation, this will return EWOULDBLOCK (windows) or
  // EINPROGRESS (linux) and we just need to wait for the socket to be writable...
  int result = ::connect(fd, (struct sockaddr *)&saddr, sizeof(saddr));
  return result == 0 || nonFatalError();
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
